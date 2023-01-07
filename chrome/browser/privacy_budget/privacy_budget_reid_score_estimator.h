// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_BUDGET_PRIVACY_BUDGET_REID_SCORE_ESTIMATOR_H_
#define CHROME_BROWSER_PRIVACY_BUDGET_PRIVACY_BUDGET_REID_SCORE_ESTIMATOR_H_

#include <list>

#include "base/containers/flat_map.h"
#include "base/sequence_checker.h"
#include "chrome/browser/privacy_budget/identifiability_study_group_settings.h"
#include "chrome/common/privacy_budget/types.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token.h"

class PrefService;

// Temporary surface-value storage to estimate the Reid score of specified
// surface set.
class PrivacyBudgetReidScoreEstimator {
 public:
  // `state_settings` and `pref_service` pointees must outlive `this`.
  explicit PrivacyBudgetReidScoreEstimator(
      const IdentifiabilityStudyGroupSettings* state_settings,
      PrefService* pref_service);

  void Init();

  PrivacyBudgetReidScoreEstimator(const PrivacyBudgetReidScoreEstimator&) =
      delete;
  PrivacyBudgetReidScoreEstimator& operator=(
      const PrivacyBudgetReidScoreEstimator&) = delete;

  ~PrivacyBudgetReidScoreEstimator();

  // Searches the storage for the surface.
  // If found, it updates its value to the token sent.
  void ProcessForReidScore(blink::IdentifiableSurface surface,
                           blink::IdentifiableToken token);

  void ResetPersistedState();

 private:
  // ReidBlockStorage is a helper class which stores a list of surfaces
  // for which we want to estimate the Reid score, i.e. corresponding to a Reid
  // block.
  class ReidBlockStorage {
   public:
    ReidBlockStorage(const IdentifiableSurfaceList& surface_list,
                     uint64_t salt_range,
                     int number_of_bits,
                     double noise_probability);
    ~ReidBlockStorage();

    ReidBlockStorage(const ReidBlockStorage&) = delete;
    ReidBlockStorage& operator=(const ReidBlockStorage&) = delete;

    ReidBlockStorage(ReidBlockStorage&&);
    ReidBlockStorage& operator=(ReidBlockStorage&&);

    // Returns whether we know the values of all the surfaces in the block.
    bool Full() const;

    // If `surface` is one of the surfaces of this block, stores `value`
    // for `surface`. If we already have a value for `surface`, the old value
    // will be discarded. Does nothing if `surface` does not belong to this
    // block.
    void Record(blink::IdentifiableSurface surface,
                blink::IdentifiableToken value);

    // Compute the hash for estimating the REID score.
    uint64_t ComputeHashForReidScore();

    // Returns the values of the surfaces. Can be called only if full.
    std::vector<blink::IdentifiableToken> GetValues() const;

    // Returns the key to be used for reporting the synthetic surface
    // corresponding to this Reid block.
    blink::IdentifiableSurface reid_surface_key() const {
      return reid_surface_key_;
    }

    // Can be called only if full.
    uint64_t salt_range() const;

    // Can be called only if full.
    int number_of_bits() const;

    // Can be called only if full.
    double noise_probability() const;

   private:
    // The surfaces which we want to track in this block, together with their
    // values (if we recorded them already).
    base::flat_map<blink::IdentifiableSurface,
                   absl::optional<blink::IdentifiableToken>>
        surfaces_;

    // Keeps track of the number of surfaces for which we have recorded a value
    // in this block.
    size_t recorded_values_count_ = 0;

    // A random salt for this block will be chosen in the interval
    // [0,`salt_range_`].
    uint64_t salt_range_;

    // The number of bits that will be reported for this block.
    int number_of_bits_;

    // The probability of reporting a random value, instead of the real hash,
    // for this block.
    double noise_probability_;

    // The surface key under which the hash for this Reid block should be
    // reported.
    blink::IdentifiableSurface reid_surface_key_;
  };

  void WriteReportedReidBlocksToPrefs() const
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Keeps track of the blocks for which we want to compute the Reid score.
  std::list<ReidBlockStorage> surface_blocks_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Keeps track of the Reid blocks which where already reported.
  IdentifiableSurfaceList already_reported_reid_blocks_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // `settings_` pointee must outlive `this`.
  raw_ptr<const IdentifiabilityStudyGroupSettings> settings_;

  // `pref_service_` pointee must outlive `this`. Used for persistent state.
  raw_ptr<PrefService> pref_service_ = nullptr;

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_BROWSER_PRIVACY_BUDGET_PRIVACY_BUDGET_REID_SCORE_ESTIMATOR_H_
