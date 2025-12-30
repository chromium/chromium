// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_FEATURES_H_
#define CHROME_BROWSER_ACTOR_ACTOR_FEATURES_H_

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "chrome/browser/page_content_annotations/multi_source_page_context_fetcher.h"

namespace actor {

BASE_DECLARE_FEATURE(kGlicActionAllowlist);

BASE_DECLARE_FEATURE_PARAM(std::string, kAllowlist);
BASE_DECLARE_FEATURE_PARAM(std::string, kAllowlistExact);
BASE_DECLARE_FEATURE_PARAM(bool, kAllowlistOnly);

BASE_DECLARE_FEATURE(kGlicActionUseOptimizationGuide);
BASE_DECLARE_FEATURE(kActorBypassTOUValidationForGuestView);

BASE_DECLARE_FEATURE(kGlicExternalProtocolActionResultCode);

BASE_DECLARE_FEATURE(kGlicBlockNavigationToDangerousContentTypes);

BASE_DECLARE_FEATURE(kGlicBlockFileSystemAccessApiFilePicker);

BASE_DECLARE_FEATURE(kGlicDeferDownloadFilePickerToUserTakeover);

BASE_DECLARE_FEATURE(kGlicCrossOriginNavigationGating);
// Feature params to kGlicCrossOriginNavigationGating to enable individual
// checks for debugging.
// Toggles if we prompt users for navigation to sensitive sites (true) or we
// just fail the navigation (false).
BASE_DECLARE_FEATURE_PARAM(bool, kGlicPromptUserForSensitiveNavigations);
// Toggles confirming actor navigations to new origins.
BASE_DECLARE_FEATURE_PARAM(bool, kGlicConfirmNavigationToNewOrigins);
// Toggles displaying a user confirmation to confirm the navigation instead of
// relying on the web client making a server call.
BASE_DECLARE_FEATURE_PARAM(bool, kGlicPromptUserForNavigationToNewOrigins);
// Toggles whether novel origin gating is based on site (true) or origin
// (false). Note that gating sensitive sites will still be origin based.
BASE_DECLARE_FEATURE_PARAM(bool, kGlicNavigationGatingUseSiteNotOrigin);
// Controls whether a hardcoded block list is enabled for the static block list.
// TODO(crbug.com/453660392): Remove flag once Component Updater rollout starts.
BASE_DECLARE_FEATURE_PARAM(bool, kGlicIncludeHardcodedBlockListEntries);

// When enabled, `beforeunload` dialog will not be displayed and the callback
// indicating the dialog outcome will be called with `true`.
// Warning: Enabling this feature can lead to data loss when navigating.
BASE_DECLARE_FEATURE(kGlicSkipBeforeUnloadDialogAndNavigate);

// When enabled, the actor will send a dialog request to the web client to
// allow the user to select a credential to use for a site. When disabled, the
// actor will automatically use the first credential.
// TODO(crbug.com/427815202): Remove this once the front end is wired up.
BASE_DECLARE_FEATURE(kGlicEnableAutoLoginDialogs);

// Kill switch for selecting previously selected credentials.
BASE_DECLARE_FEATURE(kGlicEnableAutoLoginPersistedPermissions);

// Kill switch for skipping waiting for visual state update on new tabs.
BASE_DECLARE_FEATURE(kGlicSkipAwaitVisualStateForNewTabs);

// Enables the Paint Preview backend for taking screenshots.
BASE_DECLARE_FEATURE(kGlicTabScreenshotPaintPreviewBackend);

BASE_DECLARE_FEATURE(kGlicNavigateUsingLoadURL);

BASE_DECLARE_FEATURE(kGlicNavigateToolUseOpaqueInitiator);

BASE_DECLARE_FEATURE(kGlicNavigateWithoutUserGesture);

BASE_DECLARE_FEATURE(kGlicPerformActionsReturnsBeforeStateChange);

// Enables a full page screenshot to be taken rather than only the viewport.
extern const base::FeatureParam<bool> kFullPageScreenshot;

// Controls the maximum memory/file bytes used for the capture of a single
// frame. 0 means no maximum.
extern const base::FeatureParam<size_t> kScreenshotMaxPerCaptureBytes;

// Controls whether iframe redaction is enabled, and which scope is used if so.
extern const base::FeatureParam<
    page_content_annotations::ScreenshotIframeRedactionScope>
    kScreenshotIframeRedaction;

// Kill switch for binding the created tab to the task that created it.
BASE_DECLARE_FEATURE(kActorBindCreatedTabToTask);

BASE_DECLARE_FEATURE(kActorRestartObservationDelayControllerOnNavigate);

// Kill switch to disable sending a browser signal (which is used for user
// interaction) before sending action to renderer.
BASE_DECLARE_FEATURE(kActorSendBrowserSignalForAction);

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_FEATURES_H_
