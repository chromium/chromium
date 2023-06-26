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

// `internal` code should be called outside this file with extreme caution.
// The external code should call the utility functions defined in
// chrome/browser/ui/side_panel/companion/companion_utils.h or
// chrome/browser/companion/core/utils.h.
namespace internal {
// This differs from the search companion by providing a separate WebUI that
// contains untrusted content in an iframe.
BASE_FEATURE(kSidePanelCompanion,
             "SidePanelCompanion",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Dynamically enables the search companion if the user has experiments
// enabled.
BASE_FEATURE(kCompanionEnabledByObservingExpsNavigations,
             "CompanionEnabledByObservingExpsNavigations",
             base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace internal

}  // namespace features

namespace switches {

const char kDisableCheckUserPermissionsForCompanion[] =
    "disable-checking-companion-user-permissions";

const char kForceCompanionPinnedState[] = "force-companion-pinned-state";

bool ShouldOverrideCheckingUserPermissionsForCompanion() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(kDisableCheckUserPermissionsForCompanion);
}

absl::optional<bool> ShouldForceOverrideCompanionPinState() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(kForceCompanionPinnedState)) {
    return absl::nullopt;
  }

  std::string pinned_state =
      command_line->GetSwitchValueASCII(kForceCompanionPinnedState);
  if (pinned_state == "pinned") {
    return true;
  }
  if (pinned_state == "unpinned") {
    return false;
  }

  NOTREACHED() << "Invalid Companion pin state command line switch value: "
               << pinned_state;
  return absl::nullopt;
}

}  // namespace switches
}  // namespace companion
