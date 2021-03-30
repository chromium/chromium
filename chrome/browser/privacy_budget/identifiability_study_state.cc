// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_budget/identifiability_study_state.h"

#include <algorithm>
#include <cinttypes>
#include <iterator>
#include <limits>
#include <memory>
#include <type_traits>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/crc32.h"
#include "base/numerics/ranges.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/privacy_budget/privacy_budget_prefs.h"
#include "chrome/common/privacy_budget/container_ops.h"
#include "chrome/common/privacy_budget/field_trial_param_conversions.h"
#include "chrome/common/privacy_budget/privacy_budget_features.h"
#include "chrome/common/privacy_budget/privacy_budget_settings_provider.h"
#include "components/prefs/pref_service.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings_provider.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

namespace {

// Number of entries to keep in the MRU cache. Chosen from a hat.
constexpr size_t kMruEntries = 1000;

bool IsStudyActive() {
  return blink::IdentifiabilityStudySettings::Get()->IsActive();
}

}  // namespace

constexpr int IdentifiabilityStudyState::kGeneratorVersion;

IdentifiabilityStudyState::IdentifiabilityStudyState(PrefService* pref_service)
    : pref_service_(pref_service),
      generation_(features::kIdentifiabilityStudyGeneration.Get()),
      recent_surfaces_(kMruEntries),
      surface_selection_rate_(base::ClampToRange<int>(
          features::kIdentifiabilityStudySurfaceSelectionRate.Get(),
          0,
          features::kMaxIdentifiabilityStudySurfaceSelectionRate)),
      per_surface_selection_rates_(
          DecodeIdentifiabilityFieldTrialParam<SurfaceSelectionRateMap>(
              features::kIdentifiabilityStudyPerSurfaceSettings.Get())),
      per_type_selection_rates_(
          DecodeIdentifiabilityFieldTrialParam<TypeSelectionRateMap>(
              features::kIdentifiabilityStudyPerTypeSettings.Get())),
      max_active_surfaces_(base::ClampToRange<int>(
          features::kIdentifiabilityStudyMaxSurfaces.Get(),
          0,
          features::kMaxIdentifiabilityStudyMaxSurfaces)) {
  InitializeGlobalStudySettings();
  if (IsStudyActive())
    InitFromPrefs();
}

IdentifiabilityStudyState::~IdentifiabilityStudyState() = default;

int IdentifiabilityStudyState::generation() const {
  return generation_;
}

bool IdentifiabilityStudyState::ShouldRecordSurface(
    blink::IdentifiableSurface surface) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (LIKELY(!IsStudyActive()))
    return false;

  if (base::Contains(active_surfaces_, surface))
    return true;

  // if |active_surfaces_| is saturated, then the answer to any surface not
  // contained in it is always 'no'. There's no point continuing past this
  // point.
  if (active_surfaces_.size() >= max_active_surfaces_)
    return false;

  auto recent_it = recent_surfaces_.Get(surface);
  if (recent_it != recent_surfaces_.end())
    return recent_it->second;

  // By construction, there's no intersection between blocked and
  // `active_surfaces_`. Hence we can check for blocked surfaces after checking
  // `active_surfaces_`.
  if (!blink::IdentifiabilityStudySettings::Get()->IsSurfaceAllowed(surface)) {
    recent_surfaces_.Put(surface, false);
    return false;
  }

  // A new surface. And we have capacity to add one. It's time to roll the dice:
  auto should_record = DecideSurfaceInclusion(surface);
  recent_surfaces_.Put(surface, should_record);
  if (should_record) {
    active_surfaces_.insert(surface);
    pref_service_->SetString(
        prefs::kPrivacyBudgetActiveSurfaces,
        EncodeIdentifiabilityFieldTrialParam(active_surfaces_));
  }
  return should_record;
}

// static
void IdentifiabilityStudyState::ResetGlobalStudySettingsForTesting() {
  blink::IdentifiabilityStudySettings::ResetStateForTesting();
}

// static
void IdentifiabilityStudyState::InitializeGlobalStudySettings() {
  blink::IdentifiabilityStudySettings::SetGlobalProvider(
      std::make_unique<PrivacyBudgetSettingsProvider>());
}

bool IdentifiabilityStudyState::DecideSurfaceInclusion(
    blink::IdentifiableSurface surface) {
  int selection_rate = surface_selection_rate_;

  auto rate_it = per_surface_selection_rates_.find(surface);
  if (rate_it != per_surface_selection_rates_.end()) {
    selection_rate = rate_it->second;
  } else {
    auto rate_it = per_type_selection_rates_.find(surface.GetType());
    if (rate_it != per_type_selection_rates_.end())
      selection_rate = rate_it->second;
  }

  if (selection_rate == 0)
    return false;

  if (selection_rate == 1)
    return true;

  // Actual decision is based on the prng_seed_ and the surface hash. There are
  // many more surfaces that we don't pick than those that we do. Hence it would
  // not be efficient to persist a selection decision for each and every surface
  // that we encounter.
  //
  // So instead, we use CRC32 to make a pseudo-random decision that is stable
  // for a given seed. This is the same method used in UKM
  // (See UkmRecorderImpl::IsSampledIn()).
  uint64_t surface_hash = surface.ToUkmMetricHash();
  uint32_t decision_hash =
      base::Crc32(prng_seed_, &surface_hash, sizeof(surface_hash));

  // Fair enough although not perfect.
  return decision_hash % selection_rate == 0;
}

#if DCHECK_IS_ON()
void IdentifiabilityStudyState::CheckInvariants() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!internal::Intersects(active_surfaces_, retired_surfaces_));
  DCHECK(active_surfaces_.size() <= max_active_surfaces_);
  DCHECK(std::all_of(
      active_surfaces_.begin(), active_surfaces_.end(), [](const auto& value) {
        return blink::IdentifiabilityStudySettings::Get()->IsSurfaceAllowed(
            value);
      }));
  DCHECK_NE(0u, prng_seed_);
}
#else   // DCHECK_IS_ON()
void IdentifiabilityStudyState::CheckInvariants() const {}
#endif  // DCHECK_IS_ON()

void IdentifiabilityStudyState::ResetPerReportState() {
  surface_encounters_.Reset();
}

void IdentifiabilityStudyState::ResetEphemeralState() {
  ResetPerReportState();
  recent_surfaces_.Clear();
}

void IdentifiabilityStudyState::ResetClientState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ResetEphemeralState();
  active_surfaces_.clear();
  retired_surfaces_.clear();
  pref_service_->ClearPref(prefs::kPrivacyBudgetActiveSurfaces);
  pref_service_->ClearPref(prefs::kPrivacyBudgetRetiredSurfaces);

  if (LIKELY(!IsStudyActive())) {
    prng_seed_ = 0;
    pref_service_->ClearPref(prefs::kPrivacyBudgetSeed);
    pref_service_->ClearPref(prefs::kPrivacyBudgetGeneration);
    return;
  }

  CheckAndResetPrngSeed(0u);
  pref_service_->SetInteger(prefs::kPrivacyBudgetGeneration, generation_);

  // State should be ready-to-go after resetting, even if the study is disabled.
  CheckInvariants();
}

void IdentifiabilityStudyState::InitFromPrefs() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // None of the parameters should be relied upon if the study is not enabled.
  if (LIKELY(!IsStudyActive()))
    return;

  auto persisted_generation =
      pref_service_->GetInteger(prefs::kPrivacyBudgetGeneration);
  if (persisted_generation != generation_ ||
      !pref_service_->HasPrefPath(prefs::kPrivacyBudgetSeed)) {
    ResetClientState();
    return;
  }

  // Reusing the same codec as for field trials.
  active_surfaces_ =
      DecodeIdentifiabilityFieldTrialParam<IdentifiableSurfaceSet>(
          pref_service_->GetString(prefs::kPrivacyBudgetActiveSurfaces));
  retired_surfaces_ =
      DecodeIdentifiabilityFieldTrialParam<IdentifiableSurfaceSet>(
          pref_service_->GetString(prefs::kPrivacyBudgetRetiredSurfaces));
  CheckAndResetPrngSeed(pref_service_->GetUint64(prefs::kPrivacyBudgetSeed));
  ReconcileLoadedPrefs();
}

void IdentifiabilityStudyState::WriteToPrefs() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CheckInvariants();

  // Reusing the same codec as for field trials.
  pref_service_->SetString(
      prefs::kPrivacyBudgetActiveSurfaces,
      EncodeIdentifiabilityFieldTrialParam(active_surfaces_));
  pref_service_->SetString(
      prefs::kPrivacyBudgetRetiredSurfaces,
      EncodeIdentifiabilityFieldTrialParam(retired_surfaces_));
}

void IdentifiabilityStudyState::CheckAndResetPrngSeed(uint64_t previous_value) {
  if (previous_value != 0u) {
    prng_seed_ = previous_value;
    return;
  }

  uint64_t new_seed;
  do {
    new_seed = base::RandUint64();
  } while (new_seed == 0u);
  prng_seed_ = new_seed;
  pref_service_->SetUint64(prefs::kPrivacyBudgetSeed, new_seed);
}

void IdentifiabilityStudyState::ReconcileLoadedPrefs() {
  const auto* settings = blink::IdentifiabilityStudySettings::Get();
  bool modified = false;

  // This will contain the intersection of active_surfaces_ and
  // blocked_surfaces_.
  IdentifiableSurfaceSet blocked_active_surfaces;

  // Step 1. Strip all blocked metrics from active_surfaces_. They still live on
  // in retired_surfaces_. That way a subsequent removal of the surface from the
  // blocked metrics list allows the surface to be considered for activation.
  //
  // Effectively:
  //   BlockedActive = (Active ∩ Blocked)
  //   Active -= BlockedActive
  modified |= internal::ExtractIf(&active_surfaces_, &blocked_active_surfaces,
                                  [settings](blink::IdentifiableSurface s) {
                                    return !settings->IsSurfaceAllowed(s);
                                  });

  // Step 2. Any remaining surface in both active and retired is removed from
  // retired.
  //
  // Effectively:
  //   Retired -= Active
  internal::SubtractLeftFromRight(active_surfaces_, &retired_surfaces_);

  // Step 3: Contents in active_surfaces_ in excess of max_active_surfaces_ are
  // moved to retired_surfaces_.
  //
  // Effectively:
  //   RandomActives = Random subset of Active of size |Active| - Max
  //   Active -= RandomActives
  //   Retired += RandomActives
  if (active_surfaces_.size() > max_active_surfaces_) {
    auto extracted = internal::ExtractRandomSubset(
        &active_surfaces_, &retired_surfaces_,
        active_surfaces_.size() - max_active_surfaces_);
    DCHECK(extracted);
    DCHECK_EQ(max_active_surfaces_, active_surfaces_.size());
    modified |= extracted;
  }

  // Step 4: If there is capacity, move items from retired_surfaces_ to
  //         active_surfaces_. This step is mutually exclusive from Step 3.
  //
  // Effectively:
  //   RetiredNotBlocked = Retired - Blocked
  //   RandomRetiredNotBlocked = Random subset of RetiredNotBlocked of size
  //       Max - |Active|
  //   Active += RandomRetiredNotBlocked
  //   Retired -= RandomRetiredNotBlocked
  if (active_surfaces_.size() < max_active_surfaces_ &&
      !retired_surfaces_.empty()) {
    IdentifiableSurfaceSet retired_not_blocked;
    std::copy_if(retired_surfaces_.begin(), retired_surfaces_.end(),
                 std::inserter(retired_not_blocked, retired_not_blocked.end()),
                 [settings](const blink::IdentifiableSurface& candidate) {
                   return settings->IsSurfaceAllowed(candidate);
                 });
    modified |= internal::ExtractRandomSubset(
        &retired_not_blocked, &active_surfaces_,
        max_active_surfaces_ - active_surfaces_.size());
    internal::SubtractLeftFromRight(active_surfaces_, &retired_surfaces_);
  }

  // Step 5: Merge BlockedActive into Retired
  if (!blocked_active_surfaces.empty()) {
    DCHECK(modified);
    retired_surfaces_.insert(blocked_active_surfaces.begin(),
                             blocked_active_surfaces.end());
  }

  CheckInvariants();

  if (modified)
    WriteToPrefs();
}

bool IdentifiabilityStudyState::ShouldReportEncounteredSurface(
    uint64_t source_id,
    blink::IdentifiableSurface surface) {
  if (!blink::IdentifiabilityStudySettings::Get()->IsTypeAllowed(
          blink::IdentifiableSurface::Type::kMeasuredSurface)) {
    return false;
  }
  return surface_encounters_.IsNewEncounter(source_id,
                                            surface.ToUkmMetricHash());
}
