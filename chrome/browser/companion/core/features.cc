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
// chrome/browser/ui/views/side_panel/companion/companion_utils.h or
// chrome/browser/companion/core/utils.h.
namespace internal {
// This differs from the search companion by providing a separate WebUI that
// contains untrusted content in an iframe.
// Companion can be directly enabled by either `kSidePanelCompanion` or
// `kSidePanelCompanion2`. This makes it possible for Companion to be
// enabled via multiple field trials (e.g., one that's session consistent, other
// that's permanent consistent).
BASE_FEATURE(kSidePanelCompanion,
             "SidePanelCompanion",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kSidePanelCompanion2,
             "SidePanelCompanion2",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Enables the companion on ChromeOS.
BASE_FEATURE(kSidePanelCompanionChromeOS,
             "SidePanelCompanionChromeOS",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Dynamically enables the search companion if the user has experiments
// enabled.
BASE_FEATURE(kCompanionEnabledByObservingExpsNavigations,
             "CompanionEnabledByObservingExpsNavigations",
             base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace internal

// When search companion is enabled, show a context menu item that allows the
// user to bypass the companion and open search results in a new tab.
BASE_FEATURE(kCompanionEnableSearchWebInNewTabContextMenuItem,
             "CompanionEnableSearchWebInNewTabContextMenuItem",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Allow sharing page content with CSC. Enabling this flag alone isn't enough to
// share page content - the user still needs to opt in either through a promo or
// chrome://settings. When disabled, page content will not be shared even if the
// user had previously opted in. The user won't be able to opt in (or out) when
// this is disabled.
BASE_FEATURE(kCompanionEnablePageContent,
             "CompanionEnablePageContent",
             base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace features

namespace switches {

const char kDisableCheckUserPermissionsForCompanion[] =
    "disable-checking-companion-user-permissions";

const char kForceCompanionPinnedState[] = "force-companion-pinned-state";

bool ShouldOverrideCheckingUserPermissionsForCompanion() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(kDisableCheckUserPermissionsForCompanion);
}

std::optional<bool> ShouldForceOverrideCompanionPinState() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(kForceCompanionPinnedState)) {
    return std::nullopt;
  }

  std::string pinned_state =
      command_line->GetSwitchValueASCII(kForceCompanionPinnedState);
  if (pinned_state == "pinned") {
    return true;
  }
  if (pinned_state == "unpinned") {
    return false;
  }

  NOTREACHED_IN_MIGRATION()
      << "Invalid Companion pin state command line switch value: "
      << pinned_state;
  return std::nullopt;
}

}  // namespace switches
}  // namespace companion
