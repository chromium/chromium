// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/mv2_deprecation_impact_checker.h"

#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/mv2_experiment_stage.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/manifest.h"

namespace extensions {

namespace {

// Creates and returns a singleton instance of the exception list of hashed
// extension IDs.
const std::vector<std::string>& GetHashedExceptionList() {
  static base::NoDestructor<std::vector<std::string>> g_allowlist([] {
    const std::string& string_list =
        extensions_features::kExtensionManifestV2ExceptionListParam.Get();
    return base::SplitString(string_list, ",", base::TRIM_WHITESPACE,
                             base::SPLIT_WANT_NONEMPTY);
  }());

  return *g_allowlist;
}

}  // namespace

MV2DeprecationImpactChecker::MV2DeprecationImpactChecker(
    MV2ExperimentStage experiment_stage,
    ExtensionManagement* extension_management)
    : experiment_stage_(experiment_stage),
      extension_management_(extension_management) {}
MV2DeprecationImpactChecker::~MV2DeprecationImpactChecker() = default;

bool MV2DeprecationImpactChecker::IsExtensionAffected(
    const Extension& extension) {
  return IsExtensionAffected(extension.id(), extension.manifest_version(),
                             extension.GetType(), extension.location(),
                             extension.hashed_id());
}

bool MV2DeprecationImpactChecker::IsExtensionAffected(
    const ExtensionId& extension_id,
    int manifest_version,
    Manifest::Type manifest_type,
    mojom::ManifestLocation manifest_location,
    const HashedExtensionId& hashed_id) {
  // Only consider any extensions if the experiment is enabled.
  if (experiment_stage_ == MV2ExperimentStage::kNone) {
    return false;
  }

  // Only extensions < MV3.
  if (manifest_version >= 3) {
    return false;
  }

  // Only extensions (not platform apps, etc).
  if (manifest_type != Manifest::TYPE_EXTENSION &&
      manifest_type != Manifest::TYPE_LOGIN_SCREEN_EXTENSION) {
    return false;
  }

  // Ignore component extensions (they're implementation details of Chrome).
  if (Manifest::IsComponentLocation(manifest_location)) {
    return false;
  }

  // Ignore MV2 extensions that are allowed by policy.
  if (extension_management_->IsExemptFromMV2DeprecationByPolicy(
          manifest_version, extension_id, manifest_type)) {
    return false;
  }

  // Extensions with a temporary exception.
  if (base::Contains(GetHashedExceptionList(), hashed_id.value())) {
    return false;
  }

  // The extension is an MV2 (or lower) extension; we should warn the user
  // about it.
  return true;
}

}  // namespace extensions
