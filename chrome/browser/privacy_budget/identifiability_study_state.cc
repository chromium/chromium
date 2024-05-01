// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_budget/identifiability_study_state.h"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <random>
#include <set>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/contains.h"
#include "base/containers/flat_tree.h"
#include "base/dcheck_is_on.h"
#include "base/metrics/field_trial_params.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/version_info/channel.h"
#include "chrome/browser/privacy_budget/identifiability_study_group_settings.h"
#include "chrome/browser/privacy_budget/privacy_budget_prefs.h"
#include "chrome/browser/privacy_budget/representative_surface_set.h"
#include "chrome/browser/privacy_budget/surface_set_equivalence.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/privacy_budget/field_trial_param_conversions.h"
#include "chrome/common/privacy_budget/identifiability_study_configurator.mojom.h"
#include "chrome/common/privacy_budget/privacy_budget_features.h"
#include "chrome/common/privacy_budget/privacy_budget_settings_provider.h"
#include "chrome/common/privacy_budget/types.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_switches.h"
#include "content/public/browser/render_process_host.h"
#include "ipc/ipc_channel_proxy.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings_provider.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

namespace {

int GetStudyGenerationFromFieldTrial() {
  return std::clamp(features::kIdentifiabilityStudyGeneration.Get(), 0,
                     std::numeric_limits<int>::max());
}

double GetMetaExperimentActivationProbability() {
  double settings_probability =
      features::kIdentifiabilityStudyMetaExperimentActivationProbability.Get();
  if (settings_probability < 0 || settings_probability > 1) {
    return chrome::GetChannel() == version_info::Channel::STABLE ? 0.01 : 0.5;
  } else {
    return settings_probability;
  }
}

}  // namespace

constexpr int IdentifiabilityStudyState::kGeneratorVersion;

IdentifiabilityStudyState::IdentifiabilityStudyState(PrefService* pref_service)
    : settings_(IdentifiabilityStudyGroupSettings::InitFromFeatureParams()),
      pref_service_(pref_service),
      valuation_(equivalence_),
      active_surfaces_(valuation_),
      generation_(GetStudyGenerationFromFieldTrial()),
      active_surface_budget_(settings_.surface_budget()),
      random_offset_generator_(
          settings_.expected_surface_count() > 0
              ? settings_.expected_surface_count()
              // If settings_.expected_surface_count() is 0 then the study is
              // disabled. The random offset generator will not be used.
              // However, `MesaDistribution` needs a `pivot_point` parameter
              // bigger than 0.
              : 1,
          kMesaDistributionRatio,
          kMesaDistributionGeometricDistributionParam),
      meta_experiment_active_(IsMetaExperimentActive()) {
  InitializeGlobalStudySettings();
  InitFromPrefs();
}

IdentifiabilityStudyState::~IdentifiabilityStudyState() = default;

int IdentifiabilityStudyState::generation() const {
  return generation_;
}

bool IdentifiabilityStudyState::ShouldRecordSurface(
    blink::IdentifiableSurface surface) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!settings_.enabled() && !meta_experiment_active_) [[unlikely]] {
    return false;
  }

  // We always record surfaces of type zero.
  if (surface.GetType() == blink::IdentifiableSurface::Type::kReservedInternal)
    return true;

  if (!settings_.enabled()) [[unlikely]] {
    return false;
  }

  // All other surfaces should be recorded only when sampling.
  if (!settings_.IsUsingSamplingOfSurfaces())
    return false;

  if (base::Contains(active_surfaces_, surface))
    return true;

  if (settings_.IsUsingAssignedBlockSampling())
    return false;

  DCHECK(settings_.IsUsingRandomSampling());

  if ((settings_.allowed_random_types().size() > 0) &&
      (!base::Contains(settings_.allowed_random_types(), surface.GetType()))) {
    return false;
  }

  if (!CanAddOneMoreActiveSurface())
    return false;

  if (!blink::IdentifiabilityStudySettings::Get()->ShouldSampleSurface(surface))
    return false;

  // (surface ∈ seen_surfaces_) but (surface ∉ active_surfaces_) means that
  // we've seen this surface before and decided not in include it.
  if (base::Contains(seen_surfaces_, surface))
    return false;

  return DecideInclusionForNewSurface(surface);
}

// static
void IdentifiabilityStudyState::ResetGlobalStudySettingsForTesting() {
  blink::IdentifiabilityStudySettings::ResetStateForTesting();
}

void IdentifiabilityStudyState::InitializeGlobalStudySettings() {
  blink::IdentifiabilityStudySettings::SetGlobalProvider(
      std::make_unique<PrivacyBudgetSettingsProvider>(meta_experiment_active_));
}

void IdentifiabilityStudyState::InitializeRenderer(
    content::RenderProcessHost* render_process_host) {
  IPC::ChannelProxy* channel = render_process_host->GetChannel();
  if (!channel) {
    return;
  }

  mojo::AssociatedRemote<chrome::mojom::IdentifiabilityStudyConfigurator>
      identifiability_study_configurator;
  channel->GetRemoteAssociatedInterface(&identifiability_study_configurator);
  identifiability_study_configurator->ConfigureIdentifiabilityStudy(
      /*meta_experiment_active=*/meta_experiment_active_);
}

bool IdentifiabilityStudyState::DecideInclusionForNewSurface(
    blink::IdentifiableSurface new_surface) {
  DCHECK(settings_.IsUsingRandomSampling());
  if (seen_surfaces_.size() > kMaxSelectedSurfaceOffset + 1) [[unlikely]] {
    return false;
  }

  MaybeUpdateSelectedOffsets();

  const OffsetType offset_of_new_surface = seen_surfaces_.size();
  if (!TryAddNewlySeenSurface(new_surface))
    return false;

  if (!base::Contains(selected_offsets_, offset_of_new_surface)) {
    CheckInvariants();
    return false;
  }

  if (!active_surfaces_.TryAdd(new_surface, active_surface_budget_)) {
    // Failed to add surface at offset `offset_of_surface`. So the corresponding
    // offset should be removed from `selected_offsets_` for internal
    // consistency.
    selected_offsets_.erase(offset_of_new_surface);
    WriteSelectedOffsetsToPrefs();
    CheckInvariants();
    return false;
  }

  ++active_offset_count_;
  CheckInvariants();
  return true;
}

unsigned IdentifiabilityStudyState::GetCountOfOffsetsToSelect() const {
  DCHECK(CanAddOneMoreActiveSurface());

  // We've selected `active_offset_count_` number of surfaces already.
  //
  // The number of offsets that we need to select in addition to the active
  // offsets is:
  //
  //   ExpectedSurfaceCountForCost(<remaining-budget>)
  //
  // I.e.: The expected number of surfaces we need to select in order to
  // exhaust the remaining budget.
  //
  // So the total number of offsets we need to select is:
  unsigned offset_count = active_offset_count_ +
                          SurfaceSetValuation::ExpectedSurfaceCountForCost(
                              active_surface_budget_ - active_surfaces_.Cost());

  // If the number of selected offsets is too high, any newly selected offsets
  // have too much of a probability of collision. So we cap that as well.
  //
  // Doing so means that some combinations of pivot_point() and
  // `active_surface_budget_` don't make sense‡, but don't cause harm either.
  // It's on the server-side study configuration to select sensible numbers.
  //
  // ‡ There are some budgets that cannot be saturated because it would cause
  //   too many collisions. E.g.: an `active_surface_budget_` of 1000 and
  //   a `pivot_point` of 10. Once we select around 5 surfaces, any subsequent
  //   selection has a greater than 50% chance of collision. This algorithm
  //   mitigates the damage that can be caused by such combinations. In practice
  //   the active_surface_budget_ is much smaller than the pivot_point.
  const unsigned collision_safe_upperbound =
      (random_offset_generator_.pivot_point() + 1) / 2;

  return std::min(offset_count, collision_safe_upperbound);
}

void IdentifiabilityStudyState::MaybeUpdateSelectedOffsets() {
  DCHECK(settings_.IsUsingRandomSampling());

  if (!CanAddOneMoreActiveSurface())
    return;

  const unsigned offsets_to_select = GetCountOfOffsetsToSelect();

  if (selected_offsets_.size() >= offsets_to_select)
    return;

  UpdateSelectedOffsets(offsets_to_select);
}

void IdentifiabilityStudyState::UpdateSelectedOffsets(
    unsigned offsets_to_select) {
  std::set<OffsetType> newly_selected_offsets;
  int newly_active_offset_count = 0;
  base::RandomBitGenerator bit_generator;

  while (newly_selected_offsets.size() + selected_offsets_.size() <
             offsets_to_select &&
         CanAddOneMoreActiveSurface()) {
    auto offset_to_add = random_offset_generator_.Get(bit_generator);

    // Probability of repeatedly hitting this condition is vanishingly small if
    // the following assertion holds.
    static_assert(kMaxSelectedSurfaceOffset >
                      features::kMaxIdentifiabilityStudyExpectedSurfaceCount,
                  "");
    if (offset_to_add > kMaxSelectedSurfaceOffset)
      continue;

    // Collision with previously selected offset. Kept in check with use of
    // a collision "safe" upperbound in GetCountOfOffsetsToSelect().
    auto result = newly_selected_offsets.insert(offset_to_add);
    if (!result.second || base::Contains(selected_offsets_, offset_to_add))
      continue;

    if (offset_to_add >= seen_surfaces_.size())
      continue;

    auto surface = seen_surfaces_[offset_to_add];

    if (!active_surfaces_.TryAdd(surface, active_surface_budget_)) {
      newly_selected_offsets.erase(offset_to_add);
      // If this surface didn't fit, then continuing this loop may result in
      // a perpetual loop where `active_surfaces_` is not saturated, but none of
      // the seen surfaces fit the budget.
      break;
    }

    ++newly_active_offset_count;
  }

  selected_offsets_.insert(newly_selected_offsets.begin(),
                           newly_selected_offsets.end());
  active_offset_count_ += newly_active_offset_count;
  WriteSelectedOffsetsToPrefs();
}

#if EXPENSIVE_DCHECKS_ARE_ON()
namespace {
// Predicate used in CheckInvariants().
bool IsSurfaceAllowed(const blink::IdentifiableSurface& value) {
  return blink::IdentifiabilityStudySettings::Get()->ShouldSampleSurface(value);
}
bool IsRepresentativeSurfaceAllowed(const RepresentativeSurface& value) {
  return IsSurfaceAllowed(value.value());
}
}  // namespace

void IdentifiabilityStudyState::CheckInvariants() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Should only be called if the study is active.
  DCHECK(settings_.enabled());

  // These assertions correspond to the invariants listed in
  // identifiability_study_state.h.

  // active_surfaces_
  DCHECK(
      base::ranges::all_of(active_surfaces_, IsRepresentativeSurfaceAllowed));
  DCHECK(base::ranges::all_of(selected_offsets_, [this](auto offset) {
    return offset >= seen_surfaces_.size() ||
           active_surfaces_.contains(seen_surfaces_[offset]);
  }));
  DCHECK_LE(active_surfaces_.Cost(), active_surface_budget_);

  // seen_surfaces_
  DCHECK(base::ranges::all_of(seen_surfaces_, IsSurfaceAllowed));
  DCHECK_LE(seen_surfaces_.size(),
            static_cast<size_t>(kMaxSelectedSurfaceOffset + 1));
  //                                                      ^^^
  //  For kMaxSelectedSurfaceOffset to be a valid offset into seen_surfaces_,
  //  the size of seen_surfaces_ must be at least (kMaxSelectedSurfaceOffset +
  //  1).

  // seen_surface_sequence_string_
  DCHECK_EQ(seen_surface_sequence_string_,
            EncodeIdentifiabilityFieldTrialParam(seen_surfaces_.AsList()));

  // selected_offsets_
  DCHECK(base::ranges::all_of(selected_offsets_,
                              &IdentifiabilityStudyState::IsValidOffset));

  // active_offset_count_
  DCHECK_EQ(base::ranges::count_if(selected_offsets_,
                                   [this](OffsetType offset) {
                                     return offset < seen_surfaces_.size();
                                   }),
            active_offset_count_);
}
#else   // EXPENSIVE_DCHECKS_ARE_ON()
void IdentifiabilityStudyState::CheckInvariants() const {}
#endif  // EXPENSIVE_DCHECKS_ARE_ON()

bool IdentifiabilityStudyState::CanAddOneMoreActiveSurface() const {
  return active_surfaces_.Cost() + SurfaceSetValuation::kDefaultCost <=
         active_surface_budget_;
}

void IdentifiabilityStudyState::ResetPerReportState() {
  surface_encounters_.Reset();
}

void IdentifiabilityStudyState::ResetInMemoryState() {
  ResetPerReportState();
  active_surfaces_.Clear();
  seen_surfaces_.Clear();
  seen_surface_sequence_string_.clear();
  selected_offsets_.clear();
  active_offset_count_ = 0;
  selected_block_offset_ = -1;
}

void IdentifiabilityStudyState::ResetPersistedState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ResetInMemoryState();

  pref_service_->ClearPref(prefs::kPrivacyBudgetSeenSurfaces);
  pref_service_->ClearPref(prefs::kPrivacyBudgetSelectedOffsets);
  pref_service_->ClearPref(prefs::kPrivacyBudgetSelectedBlock);

  if (!settings_.enabled()) {
    pref_service_->ClearPref(prefs::kPrivacyBudgetGeneration);
    return;
  }

  pref_service_->SetInteger(prefs::kPrivacyBudgetGeneration, generation_);

  if (settings_.IsUsingAssignedBlockSampling()) {
    InitStateForAssignedBlockSampling();
  }

  if (settings_.IsUsingRandomSampling()) {
    MaybeUpdateSelectedOffsets();
  }
  CheckInvariants();
}

void IdentifiabilityStudyState::InitStateForAssignedBlockSampling() {
  DCHECK(settings_.IsUsingAssignedBlockSampling());
  DCHECK_LT(selected_block_offset_, 0);

  IdentifiableSurfaceBlocks blocks = settings_.blocks();

  // Returning without adding anything to the active set effectively disables
  // the study.
  if (blocks.empty())
    return;

  if (pref_service_->HasPrefPath(prefs::kPrivacyBudgetSelectedBlock)) {
    selected_block_offset_ =
        pref_service_->GetInteger(prefs::kPrivacyBudgetSelectedBlock);
  }

  if (selected_block_offset_ < 0 ||
      selected_block_offset_ >= static_cast<int64_t>(blocks.size())) {
    selected_block_offset_ =
        SelectMultinomialChoice(settings_.blocks_weights());
  }

  IdentifiableSurfaceList& selected_group = blocks[selected_block_offset_];
  std::vector<OffsetType> unused_offsets;
  // If this condition winds up true, then we have an inconsistent
  // configuration.
  if (!StripDisallowedSurfaces(selected_group, unused_offsets) ||
      !unused_offsets.empty()) {
    return;
  }

  auto representative_surface_set =
      equivalence_.GetRepresentatives(selected_group);
  active_surfaces_.Assign(std::move(representative_surface_set));
  pref_service_->SetInteger(prefs::kPrivacyBudgetSelectedBlock,
                            selected_block_offset_);

  // If any single group that's specified via experiment parameters exceed the
  // experiment specified budget, then we should assume that the experiment
  // configuration is inconsistent. In this case we drop the study. It's enabled
  // (due to the experiment configuration that's only applied at startup) but
  // doesn't report anything.
  if (active_surfaces_.Cost() > active_surface_budget_)
    active_surfaces_.Clear();
}

// static
int IdentifiabilityStudyState::SelectMultinomialChoice(
    const std::vector<double>& weights) {
  std::discrete_distribution<int> distribution(weights.begin(), weights.end());
  base::RandomBitGenerator generator;
  return distribution(generator);
}

// static
bool IdentifiabilityStudyState::IsValidOffset(OffsetType ord) {
  return ord <= kMaxSelectedSurfaceOffset;
}

// static
bool IdentifiabilityStudyState::StripDisallowedSurfaces(
    IdentifiableSurfaceList& container,
    std::vector<OffsetType>& dropped_offsets) {
  const auto* settings = blink::IdentifiabilityStudySettings::Get();
  std::set<blink::IdentifiableSurface> unique_surfaces;
  OffsetType read_position = 0, write_position = 0;

  for (; read_position < container.size(); ++read_position) {
    auto surface = container[read_position];

    if (base::Contains(unique_surfaces, surface))
      return false;

    if (surface.GetType() ==
        blink::IdentifiableSurface::Type::kReservedInternal)
      return false;

    unique_surfaces.insert(surface);

    if (settings->ShouldSampleSurface(surface)) {
      container[write_position++] = surface;
    } else {
      dropped_offsets.push_back(read_position);
    }
  }
  container.resize(write_position);
  return true;
}

// static
std::vector<IdentifiabilityStudyState::OffsetType>
IdentifiabilityStudyState::AdjustForDroppedOffsets(
    std::vector<OffsetType> dropped_offsets,
    std::vector<OffsetType> offsets) {
  DCHECK(base::ranges::is_sorted(dropped_offsets));
  DCHECK(base::ranges::is_sorted(offsets));
  if (offsets.empty() || dropped_offsets.empty())
    return offsets;

  OffsetType offset_adjustment = 0;
  unsigned from_offset = 0, to_offset = 0;

  // By sticking a fake dropped offset in the back we can ensure that the range
  // of offsets from the last dropped offsets thru the last offset are also
  // handled in the same loop. Otherwise the loop will terminate at the last
  // dropped offset and we'd need additional code to go through the remainder.
  dropped_offsets.push_back(offsets.back() + 1);
  for (const auto dropped : dropped_offsets) {
    while (from_offset < offsets.size() && offsets[from_offset] < dropped)
      offsets[to_offset++] = offsets[from_offset++] - offset_adjustment;

    // Skips the element in `offsets` that corresponds to the dropped offset.
    // This is not always the case since it's valid to drop offsets that are not
    // indexed by `offsets`.
    if (from_offset < offsets.size() && offsets[from_offset] == dropped)
      ++from_offset;

    ++offset_adjustment;
  }
  offsets.resize(to_offset);
  return offsets;
}

bool IdentifiabilityStudyState::IsMetaExperimentActive() {
  if (!base::FeatureList::IsEnabled(
          features::kIdentifiabilityStudyMetaExperiment)) {
    pref_service_->ClearPref(prefs::kPrivacyBudgetMetaExperimentActivationSalt);
    return false;
  }

  // Use a fixed state when benchmarking, so that benchmarking results are more
  // reproducible.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          variations::switches::kEnableBenchmarking)) {
    return false;
  }

  // Keep the experiment consistently active or inactive for a given client.
  double salt;
  if (pref_service_->HasPrefPath(
          prefs::kPrivacyBudgetMetaExperimentActivationSalt)) {
    salt = pref_service_->GetDouble(
        prefs::kPrivacyBudgetMetaExperimentActivationSalt);
  } else {
    salt = base::RandDouble();
    pref_service_->SetDouble(prefs::kPrivacyBudgetMetaExperimentActivationSalt,
                             salt);
  }

  return salt < GetMetaExperimentActivationProbability();
}

void IdentifiabilityStudyState::InitFromPrefs() {
  if (!settings_.enabled()) [[unlikely]] {
    // Nothing to do if the study is not active. However it is possible that
    // this client has switched from active to inactive, in which case we should
    // nuke any persisted data.
    ResetPersistedState();
    return;
  }

  // Changing the generation nukes persisted state.
  auto persisted_generation =
      pref_service_->GetInteger(prefs::kPrivacyBudgetGeneration);
  if (persisted_generation != generation_) {
    ResetPersistedState();
    return;
  }

  if (settings_.IsUsingAssignedBlockSampling()) {
    InitStateForAssignedBlockSampling();
  }

  if (settings_.IsUsingRandomSampling()) {
    InitStateForRandomSurfaceSampling();
  }

  CheckInvariants();
}

void IdentifiabilityStudyState::InitStateForRandomSurfaceSampling() {
  DCHECK(settings_.IsUsingRandomSampling());
  ResetInMemoryState();

  selected_offsets_ =
      DecodeIdentifiabilityFieldTrialParam<std::vector<OffsetType>>(
          pref_service_->GetString(prefs::kPrivacyBudgetSelectedOffsets));

  // Typically we'd change kPrivacyBudgetGeneration when updating the
  // server-side experiment configuration. The following sections assume that
  // the generation has remained the same while blocked surfaces and types may
  // have changed.

  if (!base::ranges::all_of(selected_offsets_,
                            &IdentifiabilityStudyState::IsValidOffset)) {
    ResetPersistedState();
    return;
  }

  auto unfiltered_seen_surfaces =
      DecodeIdentifiabilityFieldTrialParam<IdentifiableSurfaceList>(
          pref_service_->GetString(prefs::kPrivacyBudgetSeenSurfaces));

  std::vector<OffsetType> dropped_offsets;
  if (!StripDisallowedSurfaces(unfiltered_seen_surfaces, dropped_offsets)) {
    // StripDisallowedSurfaces returns false if the input --
    // `unfiltered_seen_surfaces` in this case -- was inconsistent.
    ResetPersistedState();
    return;
  }

  seen_surfaces_ = std::move(unfiltered_seen_surfaces);

  selected_offsets_.replace(AdjustForDroppedOffsets(
      dropped_offsets, std::move(selected_offsets_).extract()));
  seen_surface_sequence_string_ =
      EncodeIdentifiabilityFieldTrialParam(seen_surfaces_);
  if (!dropped_offsets.empty())
    WriteSeenSurfacesToPrefs();
  RepresentativeSurfaceSet new_active_surfaces;
  for (auto ord : selected_offsets_) {
    if (ord < seen_surfaces_.size()) {
      new_active_surfaces.insert(
          equivalence_.GetRepresentative(seen_surfaces_[ord]));
      ++active_offset_count_;
    }
  }
  active_surfaces_.AssignWithBudget(std::move(new_active_surfaces),
                                    active_surface_budget_);

  WriteSelectedOffsetsToPrefs();
  MaybeUpdateSelectedOffsets();
}

bool IdentifiabilityStudyState::TryAddNewlySeenSurface(
    blink::IdentifiableSurface surface) {
  if (!seen_surfaces_.Add(surface))
    return false;
  if (!seen_surface_sequence_string_.empty())
    seen_surface_sequence_string_.append(",");
  seen_surface_sequence_string_.append(
      privacy_budget_internal::EncodeIdentifiabilityType(surface));
  WriteSeenSurfacesToPrefs();
  return true;
}

void IdentifiabilityStudyState::WriteSeenSurfacesToPrefs() const {
  pref_service_->SetString(prefs::kPrivacyBudgetSeenSurfaces,
                           seen_surface_sequence_string_);
}

void IdentifiabilityStudyState::WriteSelectedOffsetsToPrefs() const {
  if (selected_offsets_.empty()) {
    pref_service_->ClearPref(prefs::kPrivacyBudgetSelectedOffsets);
    return;
  }

  pref_service_->SetString(
      prefs::kPrivacyBudgetSelectedOffsets,
      EncodeIdentifiabilityFieldTrialParam(selected_offsets_));
}

bool IdentifiabilityStudyState::ShouldReportEncounteredSurface(
    uint64_t source_id,
    blink::IdentifiableSurface surface) {
  if (!blink::IdentifiabilityStudySettings::Get()->ShouldSampleType(
          blink::IdentifiableSurface::Type::kMeasuredSurface)) {
    return false;
  }

  if (surface.GetType() ==
      blink::IdentifiableSurface::Type::kReservedInternal) {
    return false;
  }

  return surface_encounters_.IsNewEncounter(source_id,
                                            surface.ToUkmMetricHash());
}
