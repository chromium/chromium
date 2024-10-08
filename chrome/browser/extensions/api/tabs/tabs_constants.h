// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used for the Tabs API and the Windows API.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_CONSTANTS_H_
#define CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_CONSTANTS_H_

namespace extensions {
namespace tabs_constants {

// Keys used in serializing tab data & events.
inline constexpr char kActiveKey[] = "active";
inline constexpr char kAllFramesKey[] = "allFrames";
inline constexpr char kAlwaysOnTopKey[] = "alwaysOnTop";
inline constexpr char kBypassCache[] = "bypassCache";
inline constexpr char kCodeKey[] = "code";
inline constexpr char kCurrentWindowKey[] = "currentWindow";
inline constexpr char kFaviconUrlKey[] = "favIconUrl";
inline constexpr char kFileKey[] = "file";
inline constexpr char kFocusedKey[] = "focused";
inline constexpr char kFormatKey[] = "format";
inline constexpr char kFromIndexKey[] = "fromIndex";
inline constexpr char kGroupIdKey[] = "groupId";
inline constexpr char kHeightKey[] = "height";
inline constexpr char kIdKey[] = "id";
inline constexpr char kIncognitoKey[] = "incognito";
inline constexpr char kIndexKey[] = "index";
inline constexpr char kLastFocusedWindowKey[] = "lastFocusedWindow";
inline constexpr char kLeftKey[] = "left";
inline constexpr char kNewPositionKey[] = "newPosition";
inline constexpr char kNewWindowIdKey[] = "newWindowId";
inline constexpr char kOldPositionKey[] = "oldPosition";
inline constexpr char kOldWindowIdKey[] = "oldWindowId";
inline constexpr char kOpenerTabIdKey[] = "openerTabId";
inline constexpr char kPinnedKey[] = "pinned";
inline constexpr char kAudibleKey[] = "audible";
inline constexpr char kDiscardedKey[] = "discarded";
inline constexpr char kAutoDiscardableKey[] = "autoDiscardable";
inline constexpr char kMutedKey[] = "muted";
inline constexpr char kMutedInfoKey[] = "mutedInfo";
inline constexpr char kQualityKey[] = "quality";
inline constexpr char kHighlightedKey[] = "highlighted";
inline constexpr char kRunAtKey[] = "runAt";
inline constexpr char kSelectedKey[] = "selected";
inline constexpr char kShowStateKey[] = "state";
inline constexpr char kStatusKey[] = "status";
inline constexpr char kTabIdKey[] = "tabId";
inline constexpr char kTabIdsKey[] = "tabIds";
inline constexpr char kTabsKey[] = "tabs";
inline constexpr char kTitleKey[] = "title";
inline constexpr char kToIndexKey[] = "toIndex";
inline constexpr char kTopKey[] = "top";
inline constexpr char kUrlKey[] = "url";
inline constexpr char kPendingUrlKey[] = "pendingUrl";
inline constexpr char kWindowClosing[] = "isWindowClosing";
inline constexpr char kWidthKey[] = "width";
inline constexpr char kWindowIdKey[] = "windowId";
inline constexpr char kWindowTypeKey[] = "type";
inline constexpr char kWindowTypeLongKey[] = "windowType";
inline constexpr char kWindowTypesKey[] = "windowTypes";
inline constexpr char kZoomSettingsMode[] = "mode";
inline constexpr char kZoomSettingsScope[] = "scope";

// Value consts.
inline constexpr char kShowStateValueNormal[] = "normal";
inline constexpr char kShowStateValueMinimized[] = "minimized";
inline constexpr char kShowStateValueMaximized[] = "maximized";
inline constexpr char kShowStateValueFullscreen[] = "fullscreen";
inline constexpr char kShowStateValueLockedFullscreen[] = "locked-fullscreen";
inline constexpr char kWindowTypeValueNormal[] = "normal";
inline constexpr char kWindowTypeValuePopup[] = "popup";
inline constexpr char kWindowTypeValueApp[] = "app";
inline constexpr char kWindowTypeValueDevTools[] = "devtools";

// Error messages.
inline constexpr char kCannotZoomDisabledTabError[] =
    "Cannot zoom a tab in disabled mode.";
inline constexpr char kCanOnlyMoveTabsWithinNormalWindowsError[] =
    "Tabs can only be moved to and from normal windows.";
inline constexpr char kCanOnlyMoveTabsWithinSameProfileError[] =
    "Tabs can only be moved between windows in the same profile.";
inline constexpr char kFrameNotFoundError[] = "No frame with id * in tab *.";
inline constexpr char kNoCrashBrowserError[] =
    "I'm sorry. I'm afraid I can't do that.";
inline constexpr char kNoCurrentWindowError[] = "No current window";
inline constexpr char kNoLastFocusedWindowError[] = "No last-focused window";
inline constexpr char kNoTabInBrowserWindowError[] =
    "There is no tab in browser window.";
inline constexpr char kInvalidTabIndexBreaksGroupContiguity[] =
    "Tab operation is invalid as the specified input would disrupt group "
    "continuity in the tab strip.";
inline constexpr char kPerOriginOnlyInAutomaticError[] =
    "Can only set scope to \"per-origin\" in \"automatic\" mode.";
inline constexpr char kWindowNotFoundError[] = "No window with id: *.";
inline constexpr char kTabIndexNotFoundError[] = "No tab at index: *.";
inline constexpr char kNotFoundNextPageError[] =
    "Cannot find a next page in history.";
inline constexpr char kTabNotFoundError[] = "No tab with id: *.";
inline constexpr char kCannotDiscardTab[] = "Cannot discard tab with id: *.";
inline constexpr char kCannotDuplicateTab[] =
    "Cannot duplicate tab with id: *.";
inline constexpr char kCannotFindTabToDiscard[] =
    "Cannot find a tab to discard.";
inline constexpr char kTabStripNotEditableError[] =
    "Tabs cannot be edited right now (user may be dragging a tab).";
inline constexpr char kTabStripNotEditableQueryError[] =
    "Tabs cannot be queried right now (user may be dragging a tab).";
inline constexpr char kTabStripDoesNotSupportTabGroupsError[] =
    "Grouping is not supported by tabs in this window.";
inline constexpr char kNoSelectedTabError[] = "No selected tab";
inline constexpr char kNoHighlightedTabError[] = "No highlighted tab";
inline constexpr char kIncognitoModeIsDisabled[] =
    "Incognito mode is disabled.";
inline constexpr char kIncognitoModeIsForced[] =
    "Incognito mode is forced. Cannot open normal windows.";
inline constexpr char kURLsNotAllowedInIncognitoError[] =
    "Cannot open URL \"*\" in an incognito window.";
inline constexpr char kInvalidUrlError[] = "Invalid url: \"*\".";
inline constexpr char kNotImplementedError[] =
    "This call is not yet implemented";
inline constexpr char kSupportedInWindowsOnlyError[] =
    "Supported in Windows only";
inline constexpr char kInvalidWindowTypeError[] = "Invalid value for type";
inline constexpr char kInvalidWindowStateError[] = "Invalid value for state";
inline constexpr char kInvalidWindowBoundsError[] =
    "Invalid value for bounds. Bounds must be at least 50% within visible "
    "screen space.";
inline constexpr char kScreenshotsDisabled[] =
    "Taking screenshots has been disabled";
inline constexpr char kScreenshotsDisabledByDlp[] =
    "Administrator policy disables screen capture when confidential content is "
    "visible";
inline constexpr char kCannotUpdateMuteCaptured[] =
    "Cannot update mute state for tab *, tab has audio or video currently "
    "being captured";
inline constexpr char kCannotDetermineLanguageOfUnloadedTab[] =
    "Cannot determine language: tab not loaded";
inline constexpr char kMissingLockWindowFullscreenPrivatePermission[] =
    "Cannot lock window to fullscreen or close a locked fullscreen window "
    "without lockWindowFullscreenPrivate manifest permission";
inline constexpr char kJavaScriptUrlsNotAllowedInExtensionNavigations[] =
    "JavaScript URLs are not allowed in API based extension navigations. Use "
    "chrome.scripting.executeScript instead.";
inline constexpr char kBrowserWindowNotAllowed[] =
    "Browser windows not allowed.";
inline constexpr char kLockedFullscreenModeNewTabError[] =
    "You cannot create new tabs while in locked fullscreen mode.";
inline constexpr char kGroupParamsError[] =
    "Cannot specify 'createProperties' along with a 'groupId'.";
inline constexpr char kCannotNavigateToDevtools[] =
    "Cannot navigate to a devtools:// page without either the devtools or "
    "debugger permission.";
inline constexpr char kCannotNavigateToChromeUntrusted[] =
    "Cannot navigate to a chrome-untrusted:// page.";
inline constexpr char kCannotHighlightTabs[] =
    "Cannot change tab highlight. This may be due to user dragging in "
    "progress.";
inline constexpr char kNotAllowedForDevToolsError[] =
    "Operation not allowed for DevTools windows";
inline constexpr char kFileUrlsNotAllowedInExtensionNavigations[] =
    "Cannot navigate to a file URL without local file access.";
inline constexpr char kWindowCreateSupportsOnlySingleIwaUrlError[] =
    "When creating a window for a URL with the 'isolated-app:' scheme, only "
    "one tab can be added to the window.";
inline constexpr char kWindowCreateCannotParseIwaUrlError[] =
    "Unable to parse 'isolated-app:' URL: %s";
inline constexpr char kWindowCreateCannotUseTabIdWithIwaError[] =
    "Creating a new window for an Isolated Web App does not support adding a "
    "tab by its ID.";
inline constexpr char kWindowCreateCannotMoveIwaTabError[] =
    "The tab of an Isolated Web App cannot be moved to a new window.";

}  // namespace tabs_constants
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_CONSTANTS_H_
