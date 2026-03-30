// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used for the Tabs API and the Windows API.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_CONSTANTS_H_
#define CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_CONSTANTS_H_

#include "build/build_config.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {
namespace tabs_constants {

// Keys used in serializing tab data & events.
inline constexpr char kActiveKey[] = "active";
inline constexpr char kFaviconUrlKey[] = "favIconUrl";
inline constexpr char kIsWindowClosingKey[] = "isWindowClosing";
inline constexpr char kSelectedKey[] = "selected";
inline constexpr char kStatusKey[] = "status";
inline constexpr char kTitleKey[] = "title";
inline constexpr char kUrlKey[] = "url";
inline constexpr char kPendingUrlKey[] = "pendingUrl";
inline constexpr char kWindowIdKey[] = "windowId";

// Error messages.
inline constexpr char kCannotZoomDisabledTabError[] =
    "Cannot zoom a tab in disabled mode.";
inline constexpr char kCannotSetZoomThisTabError[] =
    "Cannot set zoom or zoom settings on this tab.";
inline constexpr char kCannotGetZoomThisTabError[] =
    "Cannot get zoom or zoom settings on this tab.";
inline constexpr char kNoLastFocusedWindowError[] = "No last-focused window";
inline constexpr char kNoTabInBrowserWindowError[] =
    "There is no tab in browser window.";
inline constexpr char kInvalidTabIndexBreaksGroupContiguity[] =
    "Tab operation is invalid as the specified input would disrupt group "
    "continuity in the tab strip.";
inline constexpr char kPerOriginOnlyInAutomaticError[] =
    "Can only set scope to \"per-origin\" in \"automatic\" mode.";
inline constexpr char kNotFoundNextPageError[] =
    "Cannot find a next page in history.";
inline constexpr char kCannotDiscardTab[] = "Cannot discard tab with id: *.";
inline constexpr char kCannotDuplicateTab[] =
    "Cannot duplicate tab with id: *.";
inline constexpr char kNoSelectedTabError[] = "No selected tab";
inline constexpr char kIncognitoModeIsDisabled[] =
    "Incognito mode is disabled.";
inline constexpr char kIncognitoModeIsForced[] =
    "Incognito mode is forced. Cannot open normal windows.";
inline constexpr char kURLsNotAllowedInIncognitoError[] =
    "Cannot open URL \"*\" in an incognito window.";
inline constexpr char kInvalidWindowStateError[] = "Invalid value for state";
inline constexpr char kInvalidWindowBoundsError[] =
    "Invalid value for bounds. Bounds must be at least 50% within visible "
    "screen space.";
inline constexpr char kScreenshotsDisabled[] =
    "Taking screenshots has been disabled";
inline constexpr char kScreenshotsDisabledByDlp[] =
    "Administrator policy disables screen capture when confidential content is "
    "visible";
inline constexpr char kGroupParamsError[] =
    "Cannot specify 'createProperties' along with a 'groupId'.";
inline constexpr char kNotAllowedForDevToolsError[] =
    "Operation not allowed for DevTools windows";
#if BUILDFLAG(IS_ANDROID)
inline constexpr char kAndroidCannotMoveTabsWithinCctOrWebAppWindowError[] =
    "Cannot move tabs within an Android web app or custom tab window.";
inline constexpr char
    kAndroidOnlyActiveTabCanBeMovedFromCctOrWebAppWindowError[] =
        "Only the active tab in an Android web app or custom tab window can be "
        "moved from it.";
inline constexpr char kAndroidCanOnlyMoveCctOrWebAppTabsToNormalWindowError[] =
    "Tabs in an Android web app or custom tab window can only be moved to a "
    "normal browser window.";
inline constexpr char kUnableToResizeErrorAndroidSdkTooLow[] =
    "Unable to resize: unsupported Android API level";
inline constexpr char kUnableToResizeErrorAndroidBrowserRoleNotHeld[] =
    "Unable to resize: this Android app is not the primary browser";
inline constexpr char kUnableToResizeErrorAndroidNotAFreeformWindow[] =
    "Unable to resize: the Android app isn't in desktop windowing mode";
inline constexpr char kUnableToResizeErrorAndroidNullAppTask[] =
    "Unable to resize: the Android Chrome app is being run in another app's "
    "window";
inline constexpr char kUnableToResizeErrorAndroidUnsupportedOperation[] =
    "Unable to resize: operation not supported on this Android configuration";
#endif

}  // namespace tabs_constants
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_CONSTANTS_H_
