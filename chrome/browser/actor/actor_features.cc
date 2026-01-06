// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_features.h"

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace actor {

BASE_FEATURE(kGlicActionAllowlist, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(std::string,
                   kAllowlist,
                   &kGlicActionAllowlist,
                   "allowlist",
                   "");
BASE_FEATURE_PARAM(std::string,
                   kAllowlistExact,
                   &kGlicActionAllowlist,
                   "allowlist_exact",
                   "");
BASE_FEATURE_PARAM(bool,
                   kAllowlistOnly,
                   &kGlicActionAllowlist,
                   "allowlist_only",
                   true);

BASE_FEATURE(kActorBypassTOUValidationForGuestView,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kGlicActionUseOptimizationGuide, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kGlicExternalProtocolActionResultCode,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kGlicBlockNavigationToDangerousContentTypes,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGlicBlockFileSystemAccessApiFilePicker,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kGlicDeferDownloadFilePickerToUserTakeover,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kGlicCrossOriginNavigationGating,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(bool,
                   kGlicPromptUserForSensitiveNavigations,
                   &kGlicCrossOriginNavigationGating,
                   "prompt_user_for_sensitive_navigations",
                   true);
BASE_FEATURE_PARAM(bool,
                   kGlicConfirmNavigationToNewOrigins,
                   &kGlicCrossOriginNavigationGating,
                   "confirm_navigation_to_new_origins",
                   false);
BASE_FEATURE_PARAM(bool,
                   kGlicPromptUserForNavigationToNewOrigins,
                   &kGlicCrossOriginNavigationGating,
                   "prompt_user_for_navigation_to_new_origins",
                   false);
BASE_FEATURE_PARAM(bool,
                   kGlicNavigationGatingUseSiteNotOrigin,
                   &kGlicCrossOriginNavigationGating,
                   "gate_on_site_not_origin",
                   false);
BASE_FEATURE_PARAM(bool,
                   kGlicIncludeHardcodedBlockListEntries,
                   &kGlicCrossOriginNavigationGating,
                   "include_hardcoded_block_list_entries",
                   true);

BASE_FEATURE(kGlicEnableAutoLoginDialogs, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kGlicEnableAutoLoginPersistedPermissions,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kGlicSkipAwaitVisualStateForNewTabs,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kGlicTabScreenshotPaintPreviewBackend,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether to use BrowserNavigator::Navigate in NavigateTool. Fix for
// b/460113906.
BASE_FEATURE(kGlicNavigateUsingLoadURL, base::FEATURE_ENABLED_BY_DEFAULT);

// When the above NavigateWithBrowserNavigator is off, uses the legacy
// NavigateTool path but with user gesture disabled. Also a fix for b/460113906
// but with different risk profile.  No-op if above flag is on.
BASE_FEATURE(kGlicNavigateWithoutUserGesture, base::FEATURE_ENABLED_BY_DEFAULT);

// Killswitch for updating the Glic Actor API to ensure that calls to
// performAction return first when a task is stopped or paused.
BASE_FEATURE(kGlicPerformActionsReturnsBeforeStateChange,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kGlicSkipBeforeUnloadDialogAndNavigate,
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<bool> kFullPageScreenshot{
    &kGlicTabScreenshotPaintPreviewBackend, "full_page_screenshot", false};

const base::FeatureParam<size_t> kScreenshotMaxPerCaptureBytes{
    &kGlicTabScreenshotPaintPreviewBackend, "screenshot_max_per_capture_bytes",
    0};

constexpr base::FeatureParam<
    page_content_annotations::ScreenshotIframeRedactionScope>::Option
    kScreenshotIframeRedactionOptions[] = {
        {page_content_annotations::ScreenshotIframeRedactionScope::kNone,
         "none"},
        {page_content_annotations::ScreenshotIframeRedactionScope::kCrossSite,
         "cross-site"},
        {page_content_annotations::ScreenshotIframeRedactionScope::kCrossOrigin,
         "cross-origin"},
};

const base::FeatureParam<
    page_content_annotations::ScreenshotIframeRedactionScope>
    kScreenshotIframeRedaction{
        &kGlicTabScreenshotPaintPreviewBackend, "screenshot_iframe_redaction",
        page_content_annotations::ScreenshotIframeRedactionScope::kCrossSite,
        &kScreenshotIframeRedactionOptions};

BASE_FEATURE(kActorBindCreatedTabToTask, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kActorRestartObservationDelayControllerOnNavigate,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kActorSendBrowserSignalForAction,
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace actor
