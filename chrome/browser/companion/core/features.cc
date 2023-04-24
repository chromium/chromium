// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/companion/core/features.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace companion {
namespace features {

// This differs from the search companion by providing a separate WebUI that
// contains untrusted content in an iframe.
BASE_FEATURE(kSidePanelCompanion,
             "SidePanelCompanion",
             base::FEATURE_DISABLED_BY_DEFAULT);
constexpr base::FeatureParam<std::string> kHomepageURLForCompanion{
    &kSidePanelCompanion, "companion-homepage-url",
    "https://lens.google.com/companion"};

constexpr base::FeatureParam<std::string> kImageUploadURLForCompanion{
    &kSidePanelCompanion, "companion-image-upload-url",
    "https://www.example.com"};
constexpr base::FeatureParam<bool> kEnableOpenCompanionForImageSearch{
    &kSidePanelCompanion, "open-companion-for-image-search", true};
constexpr base::FeatureParam<bool> kEnableOpenCompanionForWebSearch{
    &kSidePanelCompanion, "open-companion-for-web-search", true};

}  // namespace features

namespace switches {

const char kDisableCheckUserPermissionsForCompanion[] =
    "disable-checking-companion-user-permissions";

bool ShouldOverrideCheckingUserPermissionsForCompanion() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(kDisableCheckUserPermissionsForCompanion);
}

}  // namespace switches
}  // namespace companion
