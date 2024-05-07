// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/mv2_deprecation_impact_checker.h"

#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/mv2_experiment_stage.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"

namespace extensions {

MV2DeprecationImpactChecker::MV2DeprecationImpactChecker(
    MV2ExperimentStage experiment_stage,
    ExtensionManagement* extension_management)
    : experiment_stage_(experiment_stage),
      extension_management_(extension_management) {}
MV2DeprecationImpactChecker::~MV2DeprecationImpactChecker() = default;

bool MV2DeprecationImpactChecker::IsExtensionAffected(
    const Extension& extension) {
  // Only consider any extensions if the experiment is enabled.
  if (experiment_stage_ == MV2ExperimentStage::kNone) {
    return false;
  }

  // Only extensions < MV3.
  if (extension.manifest_version() >= 3) {
    return false;
  }

  // Only extensions (not platform apps, etc).
  if (!extension.is_extension() && !extension.is_login_screen_extension()) {
    return false;
  }

  // Ignore component extensions (they're implementation details of Chrome).
  if (Manifest::IsComponentLocation(extension.location())) {
    return false;
  }

  // TODO(https://crbug.com/337191307): Finalize behavior for unpacked,
  // commandline, default-installed, OS-installed, etc extensions.

  // Ignore MV2 extensions that are allowed by policy.
  if (extension_management_->IsExemptFromMV2DeprecationByPolicy(
          extension.manifest_version(), extension.id(),
          extension.manifest()->type())) {
    return false;
  }

  // The extension is an MV2 (or lower) extension; we should warn the user
  // about it.
  return true;
}

}  // namespace extensions
