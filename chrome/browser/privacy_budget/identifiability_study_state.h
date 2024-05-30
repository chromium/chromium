// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_BUDGET_IDENTIFIABILITY_STUDY_STATE_H_
#define CHROME_BROWSER_PRIVACY_BUDGET_IDENTIFIABILITY_STUDY_STATE_H_

#include <stdint.h>

#include <cstddef>
#include <iosfwd>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "chrome/browser/privacy_budget/encountered_surface_tracker.h"
#include "chrome/browser/privacy_budget/mesa_distribution.h"
#include "chrome/browser/privacy_budget/privacy_budget_prefs.h"
#include "chrome/browser/privacy_budget/representative_surface_set.h"
#include "chrome/browser/privacy_budget/surface_set_equivalence.h"
#include "chrome/browser/privacy_budget/surface_set_valuation.h"
#include "chrome/browser/privacy_budget/surface_set_with_valuation.h"
#include "chrome/common/privacy_budget/order_preserving_set.h"
#include "chrome/common/privacy_budget/types.h"
#include "components/prefs/pref_service.h"
#include "identifiability_study_group_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

class PrefService;
class SurfaceSetEquivalence;

namespace blink {
class IdentifiableSurface;
}  // namespace blink

namespace content {
class RenderProcessHost;
}  // namespace content

namespace test_utils {
class InspectableIdentifiabilityStudyState;
}  // namespace test_utils

// Current state of the identifiability study.
//
// Persists mutable state in a `PrefService`. In normal operation the
// `PrefService` is `LocalState`. The persisted state corresponds to the prefs
// named in `privacy_budget_prefs.h`.
//
// * The list of "active" identifiable surfaces. I.e. the set of surfaces for
//   which this client is reporting sampled values.
//
// * The list of "seen" identifiable surfaces. I.e. a list of surfaces that
//   this client has seen in the order in which they were observed.
//
// In addition, this object also tracks per-session state which is not
// persisted. This state includes:
//
// * The list of "seen" surfaces that this client has reported to the server.
class IdentifiabilityStudyState {
 public:
  using OffsetType = unsigned int;

  // Construct from a `PrefService`. `pref_service` is used to retrieve and
  // store study state and MUST outlive this.
  explicit IdentifiabilityStudyState(PrefService* pref_service);

  IdentifiabilityStudyState(IdentifiabilityStudyState&) = delete;
  IdentifiabilityStudyState& operator=(const IdentifiabilityStudyState&) =
      delete;

  ~IdentifiabilityStudyState();

  // Returns the active experiment generation as defined by the server-side
  // configuration.
  //
  // See kIdentifiabilityStudyGeneration.
  int generation() const;

  // Returns true if metrics collection is enabled for `surface`.
  //
  // Calling this method may alter the state of the study settings.
  bool ShouldRecordSurface(blink::IdentifiableSurface surface);

  // Should be called from unit-tests if multiple IdentifiabilityStudyState
  // instances are to be constructed.
  static void ResetGlobalStudySettingsForTesting();

  // Returns true if tracking metrics should be recorded for this
  // source_id/surface combination.
  bool ShouldReportEncounteredSurface(uint64_t source_id,
                                      blink::IdentifiableSurface surface);

  // Resets the state associated with a single report.
  //
  // It should be called each time the UKM service constructs a UKM client
  // report.
  void ResetPerReportState();

  // Clears all persisted and ephemeral state.
  //
  // It should be called when the UKM client ID changes or if the experiment
  // generation changes.
  void ResetPersistedState();

  void InitStateForAssignedBlockSampling();
  void InitStateForRandomSurfaceSampling();

  static int SelectMultinomialChoice(const std::vector<double>& weights);

  // Initializes from fields persisted in `pref_service_`.
  void InitFromPrefs();

  // Initializes a new renderer process.
  void InitializeRenderer(content::RenderProcessHost* render_process_host);

  // The largest offset that we can select. At worst `seen_surfaces_` must keep
  // track of this many (+1) surfaces. This value is approximately based on the
  // 90ᵗʰ percentile surface encounter rate as measured in June 2021.
  static constexpr OffsetType kMaxSelectedSurfaceOffset = 1999;

  // A knob that we can use to split data sets from different versions of the
  // implementation where the differences could have material effects on the
  // data distribution.
  //
  // Increment this whenever a non-backwards-compatible change is made in the
  // code. This value is independent of any server controlled study parameters.
  static constexpr int kGeneratorVersion = 1;

  // The ratio between the linear region of the Mesa distribution and the entire
  // range. See `MesaDistribution` for details. The distribution is the source
  // of random numbers for selecting identifiable surface for measurement.
  static constexpr double kMesaDistributionRatio = 0.9;

  // The parameter of the geometric distribution used for the tail of the Mesa
  // distribution.
  static constexpr double kMesaDistributionGeometricDistributionParam = 0.5;

 private:
  friend class test_utils::InspectableIdentifiabilityStudyState;

  using SurfaceSelectionRateMap =
      base::flat_map<blink::IdentifiableSurface, int>;
  using TypeSelectionRateMap =
      base::flat_map<blink::IdentifiableSurface::Type, int>;

  // Initializes global study settings based on FeatureLists and FieldTrial
  // lists.
  void InitializeGlobalStudySettings();

  // Determines if the meta experiment must be activated for this client.
  bool IsMetaExperimentActive();

  // Checks that the invariants hold. When DCHECK_IS_ON() this call is
  // expensive. Noop otherwise.
  void CheckInvariants() const;

  // Returns true if at least one more identifiable surface can be added to the
  // active surface set. This is an estimate since each surface costs different
  // amounts.
  bool CanAddOneMoreActiveSurface() const;

  // Attempts to add `surface` to `seen_surfaces_`.
  //
  // Returns false if `surface` was already included in `seen_surfaces_` or if
  // the `seen_surfaces_` set has reached its cap. Returns true otherwise.
  bool TryAddNewlySeenSurface(blink::IdentifiableSurface surface);

  // Writes individual fields to prefs.
  void WriteSeenSurfacesToPrefs() const;
  void WriteSelectedOffsetsToPrefs() const;

  // Contains all the logic for determining whether a newly observed surface
  // should be added to the active list or not. Should only be called if
  // `active_surfaces_` does not contain `surface`.
  bool DecideInclusionForNewSurface(blink::IdentifiableSurface surface);

  // On exit, ensures that `selected_offsets_` is non-empty and satisfies our
  // invariants.
  void MaybeUpdateSelectedOffsets();

  void UpdateSelectedOffsets(unsigned expected_offset_count);

  // Resets all in-memory state, but doesn't touch any persisted state. This
  // operation invalidates the relationship between persistent and in-memory
  // states. A call to this function should be immediately followed by either
  // reading from or clearing associated preferences.
  void ResetInMemoryState();

  // Determines the number of extra offsets that should be a part of the study
  // state in order to guide surface selection.
  //
  // It attempts to answer the following question:
  //
  //    Given that `active_surfaces_.Cost()` of `active_surface_budget_` has
  //    been consumed, what's the expected number of surfaces we'd need to
  //    select in order to saturate the budget?
  //
  unsigned GetCountOfOffsetsToSelect() const;

  // Verifies that the offset `o` is within the range that's considered valid.
  // The valid range may change between versions.
  static bool IsValidOffset(OffsetType o);

  // Removes disallowed surfaces from `container` and returns the offsets of
  // removed elements relative to the original order of elements.
  //
  // Modifies `container` in-place. Appends removed offsets to `dropped_offsets`
  // in ascending order. (Note that existing offsets are not removed from
  // `container`.)
  //
  // On input, `container` should have no duplicate items nor internal
  // meta-surfaces (i.e. surfaces of type kReservedInternal). Returns `false` if
  // these conditions are violated.
  //
  // E.g.:
  //   Before:
  //       container       == {1,2,3,4}
  //       dropped_offsets == {}
  //
  //       Surface #3 (at offset 2) is blocked, and should therefore be removed.
  //
  //   After:
  //       container       == {1,2,4}
  //       dropped_offsets == {2}
  static bool StripDisallowedSurfaces(IdentifiableSurfaceList& container,
                                      std::vector<OffsetType>& dropped_offsets);

  // Given a list of offsets and a list of offsets to remove, returns the list
  // of offsets adjusted to reflect now missing offsets.
  //
  // So, for example:
  //
  //   Before:
  //     offsets = {1, 2, 3}
  //     dropped_offsets = {1}
  //   After:
  //     offsets = {1, 2} # Formerly offsets 2, and 3, but are now shifted one
  //                      # position.
  //
  //   ~ or ~
  //
  //   Before:
  //     offsets = {1,2,4,6}
  //     dropped_offsets = {2,3,5}
  //   After:
  //     offsets = {1,2,3}
  //
  static std::vector<OffsetType> AdjustForDroppedOffsets(
      std::vector<OffsetType> dropped_offsets,
      std::vector<OffsetType> offsets);

  // Wrapper around some of the experiment field trial params.
  IdentifiabilityStudyGroupSettings settings_;

  // `pref_service_` pointee must outlive `this`. Used for persistent state.
  raw_ptr<PrefService> pref_service_ = nullptr;

  // Offset of selected block. Only used when using assigned block sampling.
  //
  // Persisted in kPrivacyBudgetSelectedBlock within a single study generation.
  int selected_block_offset_ = -1;

  // `equivalence_` contains a model that determines the equivalence of
  // identifiable information for two or more surfaces. See
  // SurfaceSetEquivalence for more details.
  const SurfaceSetEquivalence equivalence_;

  // `valuation_` contains a model that determines an identifiability measure (a
  // cost or valuation, in budget parlance) for a set of identifiable surfaces.
  const SurfaceSetValuation valuation_;

  // Set of identifiable surfaces for which we will collect metrics. This set is
  // extended as we go unless it is already saturated.
  //
  // The set is considered saturated when the cost has reached
  // `active_surface_budget_`. It can also be saturated when the cost is near
  // `active_surface_budget_` but the remaining budget doesn't accommodate any
  // surface.
  //
  // Invariants:
  //
  //   * active_surfaces_ ∩ kSettings.blocked_surfaces() = Ø.
  //
  //   * s ∈ active_surfaces_ ⇒  s.GetType() ∉ kSettings.blocked_types().
  //
  //   * i ∈ selected_offsets_ ∧ i < seen_surfaces_.size()
  //                          ⇒  seen_surfaces_[i] ∈ active_surfaces_.
  //
  //   * Cost(active_surfaces_) ≤ active_surface_budget_.
  //
  // Where kSettings is the PrivacyBudgetSettingsProvider singleton.
  SurfaceSetWithValuation active_surfaces_;

  // Surfaces that the client has encountered in the order in which they were
  // encountered. The set is for fast lookup, and the list is for preserving the
  // order.
  //
  // Invariants:
  //
  //   * seen_surfaces_.CheckModel() passes.
  //
  //   * seen_surfaces_ ∩ kSettings.blocked_surfaces() = Ø.
  //
  //   * s ∈ seen_surfaces_ ⇒  s.GetType() ∉ kSettings.blocked_types().
  //
  //   * seen_surfaces_.size() <= kMaxSelectedSurfaceOffset + 1.
  //
  // Where kSettings is the PrivacyBudgetSettingsProvider singleton.
  OrderPreservingSet<blink::IdentifiableSurface> seen_surfaces_;

  // Incremental serialization of `seen_surfaces_`. Profiling indicates that as
  // the size of the list grows, the serialization consumes a non-negligible
  // amount of time during tight loops.
  //
  // Invariants:
  //
  //   * seen_surface_sequence_string_ = SerializationOf(seen_surfaces_)
  std::string seen_surface_sequence_string_;

  // Indices into `seen_surfaces_` for surfaces that are *active*.
  //
  // Only offsets that are less than |seen_surfaces_.size()| are in use. Others
  // are kept around until we have sufficient surfaces.
  //
  // Invariants:
  //
  //   * i ∈ selected_offsets_ ⇒  i <= kMaxSelectedSurfaceOffset.
  base::flat_set<OffsetType> selected_offsets_;

  // Count of offsets `i` in `selected_offsets_` which satisfy
  // `seen_surfaces_[i] ∈ active_surfaces_`.
  //
  // Invariants:
  //
  //   * active_offset_count_ = O.size() where
  //                            O = { i | i ∈ selected_offsets_ ∧
  //                                      seen_surfaces_[i] ∈ active_surfaces_}
  int active_offset_count_ = 0;

  // Contains kIdentifiabilityStudyGeneration as defined by the server-side
  // experiment.
  //
  // All valid `generation_` values are positive and non-zero. A value of zero
  // implies that the study is not active.
  const int generation_;

  // Hard cap on the number of identifiable surfaces we will sample per client.
  // The limit is specified based on the surface valuation as known to
  // SurfaceSetValuation.
  //
  // This setting can be tweaked experimentally via
  // `kIdentifiabilityStudyActiveSurfaceBudget`.
  //
  // Invariants:
  //
  //   * active_surface_budget_ ≤ kMaxIdentifiabilityStudyActiveSurfaceBudget.
  //
  const int active_surface_budget_;

  // Source of random offsets for selection. The returned offsets are in the
  // range [0, UINT_MAX]. See mesa_distribution.h for details on the random
  // distribution.
  //
  // This distribution is initialized with the expected number of surfaces as
  // the distribution's pivot point. I.e.
  // `random_offset_generator_.pivot_point()` is
  // `features::kIdentifiabilityStudyExpectedSurfaceCount`.
  MesaDistribution<OffsetType> random_offset_generator_;

  // Keeps track of which identifiable surfaces have been exposed to which UKM
  // sources. Each document and worker context within a document tree has
  // a unique source. Hence this field keeps track of identifiable surfaces
  // exposed to all execution contexts in all the document trees.
  //
  // This field resets each time a new UKM report is generated. Hence the
  // tracked value is essentially "which surfaces have been exposed to which
  // sources since the last UKM report."
  //
  // Invariants:
  //
  //   * surface_encounters_ ∩ kSettings.blocked_surfaces() = Ø.
  //
  //   * ∀ s ∈ surface_encounters_[i], s.GetType() ∉ kSettings.blocked_types().
  //
  // Where kSettings is the PrivacyBudgetSettingsProvider singleton.
  EncounteredSurfaceTracker surface_encounters_;

  // Whether the meta experiment (i.e. reporting the meta surfaces, which
  // include information only about usage of APIs) is active or not. Note that
  // this setting is independent from the rest of the Identifiability Study, and
  // can be enabled / disabled separately.
  const bool meta_experiment_active_;

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_BROWSER_PRIVACY_BUDGET_IDENTIFIABILITY_STUDY_STATE_H_
