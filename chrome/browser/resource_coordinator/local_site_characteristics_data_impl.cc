// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/local_site_characteristics_data_impl.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_database.h"
#include "chrome/browser/resource_coordinator/time.h"

namespace resource_coordinator {
namespace internal {

namespace {

// The sample weighing factor for the exponential moving averages for
// performance measurements. A factor of 1/2 gives each sample an equal weight
// to the entire previous history. As we don't know much noise there is to the
// measurement, this is essentially a shot in the dark.
// TODO(siggi): Consider adding UMA metrics to capture e.g. the fractional delta
//      from the current average, or some such.
constexpr float kSampleWeightFactor = 0.5;

base::TimeDelta GetTickDeltaSinceEpoch() {
  return NowTicks() - base::TimeTicks::UnixEpoch();
}

// Returns all the SiteDataFeatureProto elements contained in a
// SiteDataProto protobuf object.
std::vector<SiteDataFeatureProto*> GetAllFeaturesFromProto(
    SiteDataProto* proto) {
  std::vector<SiteDataFeatureProto*> ret(
      {proto->mutable_updates_favicon_in_background(),
       proto->mutable_updates_title_in_background(),
       proto->mutable_uses_audio_in_background(),
       proto->mutable_uses_notifications_in_background()});

  return ret;
}

const char* FeatureTypeToFeatureName(
    const LocalSiteCharacteristicsDataImpl::TrackedBackgroundFeatures feature) {
  switch (feature) {
    case LocalSiteCharacteristicsDataImpl::TrackedBackgroundFeatures::
        kFaviconUpdate:
      return "FaviconUpdateInBackground";
    case LocalSiteCharacteristicsDataImpl::TrackedBackgroundFeatures::
        kTitleUpdate:
      return "TitleUpdateInBackground";
    case LocalSiteCharacteristicsDataImpl::TrackedBackgroundFeatures::
        kAudioUsage:
      return "AudioUsageInBackground";
    case LocalSiteCharacteristicsDataImpl::TrackedBackgroundFeatures::
        kNotificationUsageUsage:
      return "NotificationsUsageInBackground";
  }
}

}  // namespace

void LocalSiteCharacteristicsDataImpl::NotifySiteLoaded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Update the last loaded time when this origin gets loaded for the first
  // time.
  if (loaded_tabs_count_ == 0) {
    site_characteristics_.set_last_loaded(
        TimeDeltaToInternalRepresentation(GetTickDeltaSinceEpoch()));

    is_dirty_ = true;
  }
  loaded_tabs_count_++;
}

void LocalSiteCharacteristicsDataImpl::NotifySiteUnloaded(
    performance_manager::TabVisibility tab_visibility) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (tab_visibility == performance_manager::TabVisibility::kBackground)
    DecrementNumLoadedBackgroundTabs();

  loaded_tabs_count_--;
  // Only update the last loaded time when there's no more loaded instance of
  // this origin.
  if (loaded_tabs_count_ > 0U)
    return;

  base::TimeDelta current_unix_time = GetTickDeltaSinceEpoch();

  // Update the |last_loaded_time_| field, as the moment this site gets unloaded
  // also corresponds to the last moment it was loaded.
  site_characteristics_.set_last_loaded(
      TimeDeltaToInternalRepresentation(current_unix_time));
}

void LocalSiteCharacteristicsDataImpl::NotifyLoadedSiteBackgrounded() {
  if (loaded_tabs_in_background_count_ == 0)
    background_session_begin_ = NowTicks();

  loaded_tabs_in_background_count_++;

  DCHECK_LE(loaded_tabs_in_background_count_, loaded_tabs_count_);
}

void LocalSiteCharacteristicsDataImpl::NotifyLoadedSiteForegrounded() {
  DecrementNumLoadedBackgroundTabs();
}

performance_manager::SiteFeatureUsage
LocalSiteCharacteristicsDataImpl::UpdatesFaviconInBackground() const {
  return GetFeatureUsage(
      site_characteristics_.updates_favicon_in_background(),
      GetSiteCharacteristicsDatabaseParams().favicon_update_observation_window);
}

performance_manager::SiteFeatureUsage
LocalSiteCharacteristicsDataImpl::UpdatesTitleInBackground() const {
  return GetFeatureUsage(
      site_characteristics_.updates_title_in_background(),
      GetSiteCharacteristicsDatabaseParams().title_update_observation_window);
}

performance_manager::SiteFeatureUsage
LocalSiteCharacteristicsDataImpl::UsesAudioInBackground() const {
  return GetFeatureUsage(
      site_characteristics_.uses_audio_in_background(),
      GetSiteCharacteristicsDatabaseParams().audio_usage_observation_window);
}

performance_manager::SiteFeatureUsage
LocalSiteCharacteristicsDataImpl::UsesNotificationsInBackground() const {
  return GetFeatureUsage(
      site_characteristics_.uses_notifications_in_background(),
      GetSiteCharacteristicsDatabaseParams()
          .notifications_usage_observation_window);
}

bool LocalSiteCharacteristicsDataImpl::DataLoaded() const {
  return fully_initialized_;
}

void LocalSiteCharacteristicsDataImpl::RegisterDataLoadedCallback(
    base::OnceClosure&& callback) {
  if (fully_initialized_) {
    std::move(callback).Run();
    return;
  }
  data_loaded_callbacks_.emplace_back(std::move(callback));
}

void LocalSiteCharacteristicsDataImpl::NotifyUpdatesFaviconInBackground() {
  NotifyFeatureUsage(
      site_characteristics_.mutable_updates_favicon_in_background(),
      TrackedBackgroundFeatures::kFaviconUpdate);
}

void LocalSiteCharacteristicsDataImpl::NotifyUpdatesTitleInBackground() {
  NotifyFeatureUsage(
      site_characteristics_.mutable_updates_title_in_background(),
      TrackedBackgroundFeatures::kTitleUpdate);
}

void LocalSiteCharacteristicsDataImpl::NotifyUsesAudioInBackground() {
  NotifyFeatureUsage(site_characteristics_.mutable_uses_audio_in_background(),
                     TrackedBackgroundFeatures::kAudioUsage);
}

void LocalSiteCharacteristicsDataImpl::NotifyUsesNotificationsInBackground() {
  NotifyFeatureUsage(
      site_characteristics_.mutable_uses_notifications_in_background(),
      TrackedBackgroundFeatures::kNotificationUsageUsage);
}

void LocalSiteCharacteristicsDataImpl::NotifyLoadTimePerformanceMeasurement(
    base::TimeDelta load_duration,
    base::TimeDelta cpu_usage_estimate,
    uint64_t private_footprint_kb_estimate) {
  is_dirty_ = true;

  load_duration_.AppendDatum(load_duration.InMicroseconds());
  cpu_usage_estimate_.AppendDatum(cpu_usage_estimate.InMicroseconds());
  private_footprint_kb_estimate_.AppendDatum(private_footprint_kb_estimate);
}

void LocalSiteCharacteristicsDataImpl::ExpireAllObservationWindowsForTesting() {
  auto params = GetSiteCharacteristicsDatabaseParams();
  base::TimeDelta longest_observation_window =
      std::max({params.favicon_update_observation_window,
                params.title_update_observation_window,
                params.audio_usage_observation_window,
                params.notifications_usage_observation_window});
  for (auto* iter : GetAllFeaturesFromProto(&site_characteristics_))
    IncrementFeatureObservationDuration(iter, longest_observation_window);
}

void LocalSiteCharacteristicsDataImpl::RegisterFeatureUsageCallbackForTesting(
    const TrackedBackgroundFeatures feature_type,
    base::OnceClosure callback) {
  DCHECK(
      !feature_usage_callback_for_testing_[static_cast<size_t>(feature_type)]);
  feature_usage_callback_for_testing_[static_cast<size_t>(feature_type)] =
      std::move(callback);
}

LocalSiteCharacteristicsDataImpl::LocalSiteCharacteristicsDataImpl(
    const url::Origin& origin,
    OnDestroyDelegate* delegate,
    LocalSiteCharacteristicsDatabase* database)
    : load_duration_(kSampleWeightFactor),
      cpu_usage_estimate_(kSampleWeightFactor),
      private_footprint_kb_estimate_(kSampleWeightFactor),
      origin_(origin),
      loaded_tabs_count_(0U),
      loaded_tabs_in_background_count_(0U),
      database_(database),
      delegate_(delegate),
      fully_initialized_(false),
      is_dirty_(false) {
  DCHECK(database_);
  DCHECK(delegate_);

  database_->ReadSiteCharacteristicsFromDB(
      origin_, base::BindOnce(&LocalSiteCharacteristicsDataImpl::OnInitCallback,
                              weak_factory_.GetWeakPtr()));
}

LocalSiteCharacteristicsDataImpl::~LocalSiteCharacteristicsDataImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // All users of this object should make sure that they send the same number of
  // NotifySiteLoaded and NotifySiteUnloaded events, in practice this mean
  // tracking the loaded state and sending an unload event in their destructor
  // if needed.
  DCHECK(!IsLoaded());
  DCHECK_EQ(0U, loaded_tabs_in_background_count_);

  DCHECK(delegate_);
  delegate_->OnLocalSiteCharacteristicsDataImplDestroyed(this);

  // TODO(sebmarchand): Some data might be lost here if the read operation has
  // not completed, add some metrics to measure if this is really an issue.
  if (is_dirty_ && fully_initialized_)
    database_->WriteSiteCharacteristicsIntoDB(origin_, FlushStateToProto());
}

base::TimeDelta LocalSiteCharacteristicsDataImpl::FeatureObservationDuration(
    const SiteDataFeatureProto& feature_proto) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Get the current observation duration value if available.
  base::TimeDelta observation_time_for_feature;
  if (feature_proto.has_observation_duration()) {
    observation_time_for_feature =
        InternalRepresentationToTimeDelta(feature_proto.observation_duration());
  }

  // If this site is still in background and the feature isn't in use then the
  // observation time since load needs to be added.
  if (loaded_tabs_in_background_count_ > 0U &&
      InternalRepresentationToTimeDelta(feature_proto.use_timestamp())
          .is_zero()) {
    base::TimeDelta observation_time_since_backgrounded =
        NowTicks() - background_session_begin_;
    observation_time_for_feature += observation_time_since_backgrounded;
  }

  return observation_time_for_feature;
}

// static:
void LocalSiteCharacteristicsDataImpl::IncrementFeatureObservationDuration(
    SiteDataFeatureProto* feature_proto,
    base::TimeDelta extra_observation_duration) {
  if (!feature_proto->has_use_timestamp() ||
      InternalRepresentationToTimeDelta(feature_proto->use_timestamp())
          .is_zero()) {
    feature_proto->set_observation_duration(TimeDeltaToInternalRepresentation(
        InternalRepresentationToTimeDelta(
            feature_proto->observation_duration()) +
        extra_observation_duration));
  }
}

void LocalSiteCharacteristicsDataImpl::
    ClearObservationsAndInvalidateReadOperation() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Invalidate the weak pointer that have been served, this will ensure that
  // this object doesn't get initialized from the database after being cleared.
  weak_factory_.InvalidateWeakPtrs();

  // Reset all the observations.
  site_characteristics_.Clear();

  // Clear the performance estimates, both the local state and the proto.
  cpu_usage_estimate_.Clear();
  private_footprint_kb_estimate_.Clear();
  site_characteristics_.clear_load_time_estimates();

  // Set the last loaded time to the current time if there's some loaded
  // instances of this site.
  if (IsLoaded()) {
    site_characteristics_.set_last_loaded(
        TimeDeltaToInternalRepresentation(GetTickDeltaSinceEpoch()));
  }

  // This object is now in a valid state and can be written in the database.
  TransitionToFullyInitialized();
}

performance_manager::SiteFeatureUsage
LocalSiteCharacteristicsDataImpl::GetFeatureUsage(
    const SiteDataFeatureProto& feature_proto,
    const base::TimeDelta min_obs_time) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  UMA_HISTOGRAM_BOOLEAN(
      "ResourceCoordinator.LocalDB.ReadHasCompletedBeforeQuery",
      fully_initialized_);

  // Checks if this feature has already been observed.
  // TODO(sebmarchand): Check the timestamp and reset features that haven't been
  // observed in a long time, https://crbug.com/826446.
  if (feature_proto.has_use_timestamp())
    return performance_manager::SiteFeatureUsage::kSiteFeatureInUse;

  if (FeatureObservationDuration(feature_proto) >= min_obs_time)
    return performance_manager::SiteFeatureUsage::kSiteFeatureNotInUse;

  return performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown;
}

void LocalSiteCharacteristicsDataImpl::NotifyFeatureUsage(
    SiteDataFeatureProto* feature_proto,
    const TrackedBackgroundFeatures feature_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsLoaded());
  DCHECK_GT(loaded_tabs_in_background_count_, 0U);

  // Report the observation time if this is the first time this feature is
  // observed.
  if (feature_proto->observation_duration() != 0) {
    base::UmaHistogramCustomTimes(
        base::StringPrintf(
            "ResourceCoordinator.LocalDB.ObservationTimeBeforeFirstUse.%s",
            FeatureTypeToFeatureName(feature_type)),
        InternalRepresentationToTimeDelta(
            feature_proto->observation_duration()),
        base::TimeDelta::FromSeconds(1), base::TimeDelta::FromDays(1), 100);
  }

  feature_proto->Clear();
  feature_proto->set_use_timestamp(
      TimeDeltaToInternalRepresentation(GetTickDeltaSinceEpoch()));

  if (feature_usage_callback_for_testing_[static_cast<size_t>(feature_type)]) {
    std::move(
        feature_usage_callback_for_testing_[static_cast<size_t>(feature_type)])
        .Run();
  }
}

void LocalSiteCharacteristicsDataImpl::OnInitCallback(
    base::Optional<SiteDataProto> db_site_characteristics) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Check if the initialization has succeeded.
  if (db_site_characteristics) {
    // If so, iterates over all the features and initialize them.
    auto this_features = GetAllFeaturesFromProto(&site_characteristics_);
    auto db_features =
        GetAllFeaturesFromProto(&db_site_characteristics.value());
    auto this_features_iter = this_features.begin();
    auto db_features_iter = db_features.begin();
    for (; this_features_iter != this_features.end() &&
           db_features_iter != db_features.end();
         ++this_features_iter, ++db_features_iter) {
      // If the |use_timestamp| field is set for the in-memory entry for this
      // feature then there's nothing to do, otherwise update it with the values
      // from the database.
      if (!(*this_features_iter)->has_use_timestamp()) {
        if ((*db_features_iter)->has_use_timestamp() &&
            (*db_features_iter)->use_timestamp() != 0) {
          (*this_features_iter)->Clear();
          // Keep the use timestamp from the database, if any.
          (*this_features_iter)
              ->set_use_timestamp((*db_features_iter)->use_timestamp());
        } else {
          // Else, add the observation duration from the database to the
          // in-memory observation duration.
          IncrementFeatureObservationDuration(
              (*this_features_iter),
              InternalRepresentationToTimeDelta(
                  (*db_features_iter)->observation_duration()));
        }
      }
    }
    // Only update the last loaded field if we haven't updated it since the
    // creation of this object.
    if (!site_characteristics_.has_last_loaded()) {
      site_characteristics_.set_last_loaded(
          db_site_characteristics->last_loaded());
    }
    // If there was on-disk data, update the in-memory performance averages.
    if (db_site_characteristics->has_load_time_estimates()) {
      const auto& estimates = db_site_characteristics->load_time_estimates();
      if (estimates.has_avg_load_duration_us())
        load_duration_.PrependDatum(estimates.avg_load_duration_us());
      if (estimates.has_avg_cpu_usage_us())
        cpu_usage_estimate_.PrependDatum(estimates.avg_cpu_usage_us());
      if (estimates.has_avg_footprint_kb()) {
        private_footprint_kb_estimate_.PrependDatum(
            estimates.avg_footprint_kb());
      }
    }
  }

  TransitionToFullyInitialized();
}

void LocalSiteCharacteristicsDataImpl::DecrementNumLoadedBackgroundTabs() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(loaded_tabs_in_background_count_, 0U);
  loaded_tabs_in_background_count_--;
  // Only update the observation durations if there's no more backgounded
  // instance of this origin.
  if (loaded_tabs_in_background_count_ == 0U)
    FlushFeaturesObservationDurationToProto();
}

const SiteDataProto& LocalSiteCharacteristicsDataImpl::FlushStateToProto() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Update the proto with the most current performance measurement averages.
  if (cpu_usage_estimate_.num_datums() ||
      private_footprint_kb_estimate_.num_datums()) {
    auto* estimates = site_characteristics_.mutable_load_time_estimates();
    if (load_duration_.num_datums())
      estimates->set_avg_load_duration_us(load_duration_.value());
    if (cpu_usage_estimate_.num_datums())
      estimates->set_avg_cpu_usage_us(cpu_usage_estimate_.value());
    if (private_footprint_kb_estimate_.num_datums()) {
      estimates->set_avg_footprint_kb(private_footprint_kb_estimate_.value());
    }
  }

  if (loaded_tabs_in_background_count_ > 0U)
    FlushFeaturesObservationDurationToProto();

  return site_characteristics_;
}

void LocalSiteCharacteristicsDataImpl::
    FlushFeaturesObservationDurationToProto() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!background_session_begin_.is_null());

  base::TimeTicks now = NowTicks();

  base::TimeDelta extra_observation_duration = now - background_session_begin_;
  background_session_begin_ = now;

  // Update the observation duration fields.
  for (auto* iter : GetAllFeaturesFromProto(&site_characteristics_))
    IncrementFeatureObservationDuration(iter, extra_observation_duration);
}

void LocalSiteCharacteristicsDataImpl::TransitionToFullyInitialized() {
  fully_initialized_ = true;
  for (size_t i = 0; i < data_loaded_callbacks_.size(); ++i)
    std::move(data_loaded_callbacks_[i]).Run();
  data_loaded_callbacks_.clear();
}

}  // namespace internal
}  // namespace resource_coordinator
