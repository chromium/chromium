// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_BUDGET_IDENTIFIABILITY_STUDY_GROUP_SETTINGS_H_
#define CHROME_BROWSER_PRIVACY_BUDGET_IDENTIFIABILITY_STUDY_GROUP_SETTINGS_H_

#include "chrome/common/privacy_budget/types.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

// This class wraps and validates the finch experiment parameters for the
// identifiability study. The parameters are validated at construction. If they
// are invalid, the study is disabled.
class IdentifiabilityStudyGroupSettings {
 public:
  // Constructs the settings from the values of the feature params.
  static IdentifiabilityStudyGroupSettings InitFromFeatureParams();

  // Constructs the settings from the explicit values passed below. For the
  // meaning of the parameters, see
  // chrome/common/privacy_budget/privacy_budget_features.h.
  static IdentifiabilityStudyGroupSettings InitFrom(
      bool enabled,
      int expected_surface_count,
      int surface_budget,
      const std::string& blocks,
      const std::string& blocks_weights,
      const std::string& allowed_random_types);

  IdentifiabilityStudyGroupSettings(const IdentifiabilityStudyGroupSettings&) =
      delete;
  IdentifiabilityStudyGroupSettings(IdentifiabilityStudyGroupSettings&&);
  ~IdentifiabilityStudyGroupSettings();

  IdentifiabilityStudyGroupSettings& operator=(
      const IdentifiabilityStudyGroupSettings&) const = delete;
  IdentifiabilityStudyGroupSettings& operator=(
      const IdentifiabilityStudyGroupSettings&&) const = delete;

  // Whether the study should be enabled.
  bool enabled() const { return enabled_; }

  bool IsUsingAssignedBlockSampling() const;
  bool IsUsingRandomSampling() const;

  // Whether the study is using one of the sampling strategies (random or block
  // assignment).
  bool IsUsingSamplingOfSurfaces() const;

  const IdentifiableSurfaceBlocks& blocks() const;
  const std::vector<double>& blocks_weights() const;
  const std::vector<blink::IdentifiableSurface::Type>& allowed_random_types()
      const;

  int expected_surface_count() const { return expected_surface_count_; }
  int surface_budget() const { return surface_budget_; }

 private:
  IdentifiabilityStudyGroupSettings(
      bool enabled,
      int surface_count,
      int surface_budget,
      IdentifiableSurfaceBlocks blocks,
      std::vector<double> blocks_weights,
      std::vector<blink::IdentifiableSurface::Type> allowed_random_types);

  bool Validate();
  bool ValidateAssignedBlockSampling();

  // True if identifiability study is enabled. If this field is false, then none
  // of the other values are applicable.
  bool enabled_;

  const int expected_surface_count_;

  const int surface_budget_;

  const IdentifiableSurfaceBlocks blocks_;

  const std::vector<double> blocks_weights_;

  // Surface types to sample from when random surface sampling is enabled. If
  // this vector is empty all surface types are allowed to be sampled.
  const std::vector<blink::IdentifiableSurface::Type> allowed_random_types_;
};

#endif  // CHROME_BROWSER_PRIVACY_BUDGET_IDENTIFIABILITY_STUDY_GROUP_SETTINGS_H_
