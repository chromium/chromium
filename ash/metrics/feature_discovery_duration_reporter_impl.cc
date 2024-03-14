// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/feature_discovery_duration_reporter_impl.h"

#include "ash/public/cpp/feature_discovery_metric_util.h"
#include "ash/shell.h"
#include "base/containers/contains.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "ui/display/screen.h"

namespace ash {

namespace {

// Parameters used by the time duration metrics.
constexpr base::TimeDelta kTimeMetricsMin = base::Seconds(1);
constexpr base::TimeDelta kTimeMetricsMax = base::Days(7);
constexpr int kTimeMetricsBucketCount = 100;

// A dictionary that maps the features observed by
// `FeatureDiscoveryDurationReporter` to observation data (which is also a
// dictionary. See observation data dictionary keys for more details). A
// key-value mapping is added to the dictionary when the observation on a
// feature starts. The entries of the dictionary are never deleted after
// addition. It helps to avoid duplicate recordings on the same feature.
// NOTE: since it is a pref service key, do not change its value.
constexpr char kObservedFeatures[] = "FeatureDiscoveryReporterObservedFeatures";

// Observation data dictionary keys --------------------------------------------

// The key to the cumulated time duration since the onbservation starts. This
// key and its paired value get cleared when the observation finishes.
// NOTE: since it is a pref service key, do not change its value.
constexpr char kCumulatedDuration[] = "cumulative_duration";

// The key to the boolean value that indicates whether the observation finishes.
// NOTE: since it is a pref service key, do not change its value.
constexpr char kIsObservationFinished[] = "is_observation_finished";

// The key to the boolean value that is true if the observation starts in
// tablet. This key should only be used when the metrics data collected from a
// tracked feature should be split by tablet mode.
// NOTE: since it is a pref service key, do not change its value.
constexpr char kActivatedInTablet[] = "activated_in_tablet";

// Helper functions ------------------------------------------------------------

void ReportFeatureDiscoveryDuration(const char* histogram,
                                    const base::TimeDelta& duration) {
  base::UmaHistogramCustomTimes(histogram, duration, kTimeMetricsMin,
                                kTimeMetricsMax, kTimeMetricsBucketCount);
}

// Returns a trackable feature's info.
const feature_discovery::TrackableFeatureInfo& FindMappedFeatureInfo(
    feature_discovery::TrackableFeature feature) {
  auto iter =
      base::ranges::find(feature_discovery::kTrackableFeatureArray, feature,
                         &feature_discovery::TrackableFeatureInfo::feature);
  DCHECK(feature_discovery::kTrackableFeatureArray.cend() != iter);
  return *iter;
}

// Returns a trackable feature's name.
const char* FindMappedName(feature_discovery::TrackableFeature feature) {
  return FindMappedFeatureInfo(feature).name;
}

// Calculates the histogram for metric reporting. `feature` specifies a
// trackable feature. `in_tablet` is true if the observation on `feature` is
// activated in tablet.
// NOTE: if the metric reporting for `feature` is not separated by tablet mode,
// `in_tablet` is null.
const char* CalculateHistogram(feature_discovery::TrackableFeature feature,
                               std::optional<bool> in_tablet) {
  const feature_discovery::TrackableFeatureInfo& info =
      FindMappedFeatureInfo(feature);
  if (!info.split_by_tablet_mode)
    return info.histogram;

  DCHECK(in_tablet);
  return *in_tablet ? info.histogram_tablet : info.histogram_clamshell;
}

}  // namespace

FeatureDiscoveryDurationReporterImpl::FeatureDiscoveryDurationReporterImpl(
    SessionController* session_controller) {
  session_controller_observation_.Observe(session_controller);
}

FeatureDiscoveryDurationReporterImpl::~FeatureDiscoveryDurationReporterImpl() {
  // Handle the case when a user signs out all accounts. Store the states of
  // the ongoing observations through the pref service.
  SetActive(false);
}

// static
void FeatureDiscoveryDurationReporterImpl::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kObservedFeatures);
}

void FeatureDiscoveryDurationReporterImpl::MaybeActivateObservation(
    feature_discovery::TrackableFeature feature) {
  if (!is_active())
    return;

  const base::Value::Dict& observed_features =
      active_pref_service_->GetDict(kObservedFeatures);

  // If `feature` is already under observation, return early.
  // TODO(https://crbug.com/1311344): implement the option that allows the
  // observation start time gets reset by the subsequent observation
  // activation callings.
  const feature_discovery::TrackableFeatureInfo& info =
      FindMappedFeatureInfo(feature);
  const char* feature_name = info.name;
  if (observed_features.Find(feature_name))
    return;

  // Initialize the pref data for the new observation.
  base::Value::Dict observed_feature_data;
  observed_feature_data.Set(kCumulatedDuration,
                            base::TimeDeltaToValue(base::TimeDelta()));
  observed_feature_data.Set(kIsObservationFinished, false);
  if (info.split_by_tablet_mode) {
    // Record the current tablet mode if `feature`'s discovery duration data
    // should be separated by tablet mode.
    observed_feature_data.Set(kActivatedInTablet,
                              display::Screen::GetScreen()->InTabletMode());
  }

  ScopedDictPrefUpdate update(active_pref_service_, kObservedFeatures);
  update->Set(feature_name, std::move(observed_feature_data));

  // Record observation start time.
  DCHECK(!base::Contains(active_time_recordings_, feature));
  active_time_recordings_.emplace(feature, base::TimeTicks::Now());
}

void FeatureDiscoveryDurationReporterImpl::MaybeFinishObservation(
    feature_discovery::TrackableFeature feature) {
  if (!is_active())
    return;

  // If the observation on the given metric has not started yet, return early.
  auto iter = active_time_recordings_.find(feature);
  if (iter == active_time_recordings_.end())
    return;

  const base::Value::Dict& observed_features =
      active_pref_service_->GetDict(kObservedFeatures);
  const char* const feature_name = FindMappedName(feature);
  const base::Value::Dict* feature_pref_data =
      observed_features.Find(feature_name)->GetIfDict();
  DCHECK(feature_pref_data);

  const std::optional<base::TimeDelta> accumulated_duration =
      base::ValueToTimeDelta(feature_pref_data->Find(kCumulatedDuration));
  DCHECK(accumulated_duration);

  bool skip_report = false;

  // Get the boolean that indicates under which mode (clamshell or tablet) the
  // observation is activated. If the metric data should not be separated, the
  // value is null.
  std::optional<bool> activated_in_tablet;
  if (FindMappedFeatureInfo(feature).split_by_tablet_mode) {
    activated_in_tablet = feature_pref_data->FindBool(kActivatedInTablet);
    DCHECK(activated_in_tablet);

    // It is abnormal to miss `activated_in_tablet`. Handle this case for
    // safety. Skip metric reporting if `activated_in_tablet` is missing when
    // the metric data should be split by tablet mode. One reason leading to
    // this case is that a feature switches from non-split to tablet-mode-split
    // due to later code changes.
    if (!activated_in_tablet) {
      LOG(ERROR) << "Cannot find the tablet mode state under which the feature "
                    "observation starts for "
                 << FindMappedName(feature);
      skip_report = true;
    }
  }

  // Report metric data if there is no errors.
  if (!skip_report) {
    ReportFeatureDiscoveryDuration(
        CalculateHistogram(feature, activated_in_tablet),
        *accumulated_duration + base::TimeTicks::Now() - iter->second);
  }

  // Update the observed feature pref data by:
  // 1. Clearing the cumulated duration
  // 2. Marking that the observation finishes
  // 3. Erasing the saved tablet state if any
  ScopedDictPrefUpdate update(active_pref_service_, kObservedFeatures);
  base::Value::Dict* mutable_feature_pref_data = update->FindDict(feature_name);
  mutable_feature_pref_data->Remove(kCumulatedDuration);
  mutable_feature_pref_data->Set(kIsObservationFinished, true);
  mutable_feature_pref_data->Remove(kActivatedInTablet);

  active_time_recordings_.erase(iter);
}

void FeatureDiscoveryDurationReporterImpl::AddObserver(
    ReporterObserver* observer) {
  observers_.AddObserver(observer);
}

void FeatureDiscoveryDurationReporterImpl::RemoveObserver(
    ReporterObserver* observer) {
  observers_.RemoveObserver(observer);
}

void FeatureDiscoveryDurationReporterImpl::SetActive(bool active) {
  // Return early if:
  // 1. the activity state does not change; or
  // 2. `active_pref_service_` is not set.
  if (active == is_active() || !active_pref_service_)
    return;

  if (!active) {
    Deactivate();
    return;
  }

  Activate();
}

void FeatureDiscoveryDurationReporterImpl::Activate() {
  // Disable the reporter for secondary accounts so that the feature discovery
  // duration is only reported on primary accounts.
  if (!Shell::Get()->session_controller()->IsUserPrimary())
    return;

  // Verify data members before activation.
  DCHECK(active_time_recordings_.empty());
  DCHECK(!is_active_);
  DCHECK(active_pref_service_);

  is_active_ = true;
  const base::Value::Dict& observed_features =
      active_pref_service_->GetDict(kObservedFeatures);
  const base::Value::Dict& immutable_observed_features_dict = observed_features;

  // Iterate trackable features and resume unfinished observations.
  for (const auto& feature_info : feature_discovery::kTrackableFeatureArray) {
    // Skip the features that are not under observation.
    const base::Value* feature_data =
        immutable_observed_features_dict.Find(feature_info.name);
    if (!feature_data)
      continue;

    // Skip the finished observations.
    std::optional<bool> is_finished =
        feature_data->GetDict().FindBool(kIsObservationFinished);
    DCHECK(is_finished);
    if (*is_finished)
      continue;

    active_time_recordings_.emplace(feature_info.feature,
                                    base::TimeTicks::Now());
  }

  for (ReporterObserver& observer : observers_)
    observer.OnReporterActivated();
}

void FeatureDiscoveryDurationReporterImpl::Deactivate() {
  if (!active_time_recordings_.empty()) {
    ScopedDictPrefUpdate update(active_pref_service_, kObservedFeatures);
    base::Value::Dict& mutable_observed_features_dict = update.Get();

    // Store the accumulated time duration as pref data.
    for (const auto& name_timestamp_pair : active_time_recordings_) {
      // Fetch cumulated duration from pref service.
      const char* feature_name = FindMappedName(name_timestamp_pair.first);
      base::Value* feature_data =
          mutable_observed_features_dict.Find(feature_name);
      DCHECK(feature_data);
      base::Value::Dict& mutable_data_dict = feature_data->GetDict();
      const base::Value* cumulated_duration_value =
          mutable_data_dict.Find(kCumulatedDuration);
      DCHECK(cumulated_duration_value);
      std::optional<base::TimeDelta> cumulated_duration =
          base::ValueToTimeDelta(cumulated_duration_value);
      DCHECK(cumulated_duration);

      // Add the observation duration under the current active session. Then
      // store the total duration.
      mutable_data_dict.Set(
          kCumulatedDuration,
          base::TimeDeltaToValue(*cumulated_duration + base::TimeTicks::Now() -
                                 name_timestamp_pair.second));
    }

    active_time_recordings_.clear();
  }

  is_active_ = false;
}

void FeatureDiscoveryDurationReporterImpl::OnSessionStateChanged(
    session_manager::SessionState state) {
  SetActive(state == session_manager::SessionState::ACTIVE);
}

void FeatureDiscoveryDurationReporterImpl::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  // Halt the observations for the old active account if any.
  if (is_active())
    SetActive(false);

  active_pref_service_ = pref_service;
  SetActive(Shell::Get()->session_controller()->GetSessionState() ==
            session_manager::SessionState::ACTIVE);
}

}  // namespace ash
