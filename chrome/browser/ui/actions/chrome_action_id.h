// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ACTIONS_CHROME_ACTION_ID_H_
#define CHROME_BROWSER_UI_ACTIONS_CHROME_ACTION_ID_H_

#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "ui/actions/action_id.h"

// The references to the IDC_XXXX command ids is intended purely for
// documentation purposes in order to maintain the correlation between the new
// action id and the legacy command id. NOTE: The ordinal values will *not* be
// the same. Eventually, these references may be removed once the transition to
// pure ActionItems is complete.

// clang-format off
#define CHROME_COMMON_ACTION_IDS \
  /* Navigation commands */ \
  E(kActionBack, IDC_BACK, kChromeActionsStart, ChromeActionIds) \
  E(kActionReload, IDC_RELOAD) \
  E(kActionOpenCurrentUrl, IDC_OPEN_CURRENT_URL) \
  E(kActionStop, IDC_STOP) \
  E(kActionReloadBypassingCache, IDC_RELOAD_BYPASSING_CACHE) \
  E(kActionReloadClearingCache, IDC_RELOAD_CLEARING_CACHE) \
  /* Window management commands */ \
  E(kActionNewWindow, IDC_NEW_WINDOW) \
  E(kActionCloseWindow, IDC_CLOSE_WINDOW) \
  E(kActionNewTab, IDC_NEW_TAB) \
  E(kActionCloseTab, IDC_CLOSE_TAB) \
  E(kActionSelectNextTab, IDC_SELECT_NEXT_TAB) \
  E(kActionSelectPreviousTab, IDC_SELECT_PREVIOUS_TAB) \
  E(kActionSelectTab0, IDC_SELECT_TAB_0) \
  E(kActionSelectTab1, IDC_SELECT_TAB_1) \
  E(kActionSelectTab2, IDC_SELECT_TAB_2) \
  E(kActionSelectTab3, IDC_SELECT_TAB_3) \
  E(kActionSelectTab4, IDC_SELECT_TAB_4) \
  E(kActionSelectTab5, IDC_SELECT_TAB_5) \
  E(kActionSelectTab6, IDC_SELECT_TAB_6) \
  E(kActionSelectTab7, IDC_SELECT_TAB_7) \
  E(kActionSelectLastTab, IDC_SELECT_LAST_TAB) \
  E(kActionDuplicateTab, IDC_DUPLICATE_TAB) \
  E(kActionRestoreTab, IDC_RESTORE_TAB) \
  E(kActionShowAsTab, IDC_SHOW_AS_TAB) \
  E(kActionFullscreen, IDC_FULLSCREEN) \
  E(kActionExit, IDC_EXIT) \
  E(kActionMoveTabNext, IDC_MOVE_TAB_NEXT) \
  E(kActionMoveTabPrevious, IDC_MOVE_TAB_PREVIOUS) \
  E(kActionSearch, IDC_SEARCH) \
  E(kActionMinimizeWindow, IDC_MINIMIZE_WINDOW) \
  E(kActionMaximizeWindow, IDC_MAXIMIZE_WINDOW) \
  E(kActionNameWindow, IDC_NAME_WINDOW) \
  E(kActionOpenInPwaWindow, IDC_OPEN_IN_PWA_WINDOW) \
  E(kActionMoveTabToNewWindow, IDC_MOVE_TAB_TO_NEW_WINDOW) \
  /* Web app window commands */ \
  E(kActionOpenInChrome, IDC_OPEN_IN_CHROME) \
  E(kActionWebAppSettings, IDC_WEB_APP_SETTINGS) \
  E(kActionWebAppMenuAppInfo, IDC_WEB_APP_MENU_APP_INFO) \
  /* Page-related commands */ \
  E(kActionBookmarkThisTab, IDC_BOOKMARK_THIS_TAB) \
  E(kActionBookmarkAllTabs, IDC_BOOKMARK_ALL_TABS) \
  E(kActionViewSource, IDC_VIEW_SOURCE) \
  E(kActionSavePage, IDC_SAVE_PAGE) \
  E(kActionEmailPageLocation, IDC_EMAIL_PAGE_LOCATION) \
  E(kActionBasicPrint, IDC_BASIC_PRINT) \
  E(kActionWindowMuteSite, IDC_WINDOW_MUTE_SITE) \
  E(kActionWindowPinTab, IDC_WINDOW_PIN_TAB) \
  E(kActionWindowGroupTab, IDC_WINDOW_GROUP_TAB) \
  E(kActionFocusThisTab, IDC_FOCUS_THIS_TAB) \
  E(kActionWindowCloseTabsToRight, IDC_WINDOW_CLOSE_TABS_TO_RIGHT) \
  E(kActionWindowCloseOtherTabs, IDC_WINDOW_CLOSE_OTHER_TABS) \
  E(kActionNewTabToRight, IDC_NEW_TAB_TO_RIGHT) \
  E(kActionSaveAutofillAddress, IDC_SAVE_AUTOFILL_ADDRESS) \
  E(kActionOffersAndRewardsForPage, IDC_OFFERS_AND_REWARDS_FOR_PAGE) \
  E(kActionSharingHub, IDC_SHARING_HUB) \
  E(kActionFilledCardInformation, IDC_FILLED_CARD_INFORMATION) \
  E(kActionSharingHubScreenshot, IDC_SHARING_HUB_SCREENSHOT) \
  E(kActionVirtualCardEnroll, IDC_VIRTUAL_CARD_ENROLL) \
  E(kActionAutofillMandatoryReauth, IDC_AUTOFILL_MANDATORY_REAUTH) \
  E(kActionShowMemorySaverChip) \
  E(kActionShowJsOptimizationsIcon) \
  E(kActionShowCookieControls) \
  E(kActionUnfocusTabGroup, IDC_UNFOCUS_TAB_GROUP) \
  E(kActionAddNewTabToGroup, IDC_ADD_NEW_TAB_TO_GROUP) \
  E(kActionCreateNewTabGroup, IDC_CREATE_NEW_TAB_GROUP) \
  E(kActionFocusNextTabGroup, IDC_FOCUS_NEXT_TAB_GROUP) \
  E(kActionFocusPrevTabGroup, IDC_FOCUS_PREV_TAB_GROUP) \
  E(kActionCloseTabGroup, IDC_CLOSE_TAB_GROUP) \
  E(kActionGroupUngroupedTabs, IDC_GROUP_UNGROUPED_TABS) \
  E(kActionCreateNewTabGroupTopLevel, IDC_CREATE_NEW_TAB_GROUP_TOP_LEVEL) \
  E(kActionAddNewTabRecentGroup, IDC_ADD_NEW_TAB_RECENT_GROUP) \
  E(kActionFakePageActionForDebug, IDC_FAKE_PAGE_ACTION_FOR_DEBUG) \
  /* Page-manipulation commands that target a specified tab, which may not */ \
  /* be the active one. */ \
  E(kActionMuteTargetSite, IDC_MUTE_TARGET_SITE) \
  E(kActionPinTargetTab, IDC_PIN_TARGET_TAB) \
  E(kActionGroupTargetTab, IDC_GROUP_TARGET_TAB) \
  E(kActionDuplicateTargetTab, IDC_DUPLICATE_TARGET_TAB) \
  /* Find-in-page */ \
  E(kActionFind, IDC_FIND) \
  E(kActionFindNext, IDC_FIND_NEXT) \
  E(kActionFindPrevious, IDC_FIND_PREVIOUS) \
  E(kActionCloseFindOrStop, IDC_CLOSE_FIND_OR_STOP) \
  /* Profile sub menu */ \
  E(kActionCustomizeChrome, IDC_CUSTOMIZE_CHROME) \
  E(kActionCloseProfile, IDC_CLOSE_PROFILE) \
  E(kActionManageGoogleAccount, IDC_MANAGE_GOOGLE_ACCOUNT) \
  E(kActionShowSyncSettings, IDC_SHOW_SYNC_SETTINGS) \
  E(kActionTurnOnSync, IDC_TURN_ON_SYNC) \
  E(kActionShowSigninWhenPaused, IDC_SHOW_SIGNIN_WHEN_PAUSED) \
  E(kActionOpenGuestProfile, IDC_OPEN_GUEST_PROFILE) \
  E(kActionAddNewProfile, IDC_ADD_NEW_PROFILE) \
  E(kActionManageChromeProfiles, IDC_MANAGE_CHROME_PROFILES) \
  E(kActionShowSignin, IDC_SHOW_SIGNIN) \
  /* Zoom */ \
  E(kActionZoomPlus, IDC_ZOOM_PLUS) \
  E(kActionZoomNormal, IDC_ZOOM_NORMAL) \
  E(kActionZoomMinus, IDC_ZOOM_MINUS) \
  /* Focus various bits of UI */ \
  E(kActionFocusToolbar, IDC_FOCUS_TOOLBAR) \
  E(kActionFocusLocation, IDC_FOCUS_LOCATION) \
  E(kActionFocusSearch, IDC_FOCUS_SEARCH) \
  E(kActionFocusMenuBar, IDC_FOCUS_MENU_BAR) \
  E(kActionFocusNextPane, IDC_FOCUS_NEXT_PANE) \
  E(kActionFocusPreviousPane, IDC_FOCUS_PREVIOUS_PANE) \
  E(kActionFocusBookmarks, IDC_FOCUS_BOOKMARKS) \
  E(kActionFocusInactivePopupForAccessibility, \
    IDC_FOCUS_INACTIVE_POPUP_FOR_ACCESSIBILITY) \
  E(kActionFocusWebContentsPane, IDC_FOCUS_WEB_CONTENTS_PANE) \
  /* Show various bits of UI */ \
  E(kActionOpenFile, IDC_OPEN_FILE) \
  E(kActionCreateShortcut, IDC_CREATE_SHORTCUT) \
  E(kActionDevToolsConsole, IDC_DEV_TOOLS_CONSOLE) \
  E(kActionDevToolsDevices, IDC_DEV_TOOLS_DEVICES) \
  E(kActionFeedback, IDC_FEEDBACK) \
  E(kActionShowBookmarkBar, IDC_SHOW_BOOKMARK_BAR) \
  E(kActionShowHistory, IDC_SHOW_HISTORY) \
  E(kActionShowBookmarkManager, IDC_SHOW_BOOKMARK_MANAGER) \
  E(kActionImportSettings, IDC_IMPORT_SETTINGS) \
  E(kActionOptions, IDC_OPTIONS) \
  E(kActionEditSearchEngines, IDC_EDIT_SEARCH_ENGINES) \
  E(kActionViewPasswords, IDC_VIEW_PASSWORDS) \
  E(kActionAbout, IDC_ABOUT) \
  E(kActionHelpPageViaKeyboard, IDC_HELP_PAGE_VIA_KEYBOARD) \
  E(kActionHelpPageViaMenu, IDC_HELP_PAGE_VIA_MENU) \
  E(kActionShowAppMenu, IDC_SHOW_APP_MENU) \
  E(kActionManageExtensions, IDC_MANAGE_EXTENSIONS) \
  E(kActionDevToolsInspect, IDC_DEV_TOOLS_INSPECT) \
  E(kActionUpgradeDialog, IDC_UPGRADE_DIALOG) \
  E(kActionSetBrowserAsDefault, IDC_SET_BROWSER_AS_DEFAULT) \
  E(kActionProfilingEnabled, IDC_PROFILING_ENABLED) \
  E(kActionExtensionErrors, IDC_EXTENSION_ERRORS) \
  E(kActionShowAvatarMenu, IDC_SHOW_AVATAR_MENU) \
  E(kActionExtensionInstallErrorFirst, IDC_EXTENSION_INSTALL_ERROR_FIRST) \
  E(kActionToggleRequestTabletSite, IDC_TOGGLE_REQUEST_TABLET_SITE) \
  E(kActionDevToolsToggle, IDC_DEV_TOOLS_TOGGLE) \
  E(kActionTakeScreenshot, IDC_TAKE_SCREENSHOT) \
  E(kActionToggleFullscreenToolbar, IDC_TOGGLE_FULLSCREEN_TOOLBAR) \
  E(kActionCustomizeTouchBar, IDC_CUSTOMIZE_TOUCH_BAR) \
  E(kActionShowBetaForum, IDC_SHOW_BETA_FORUM) \
  E(kActionToggleJavascriptAppleEvents, IDC_TOGGLE_JAVASCRIPT_APPLE_EVENTS) \
  E(kActionInstallPwa, IDC_INSTALL_PWA) \
  E(kActionShowCollaborationRecentActivity) \
  E(kActionShowManagementPage, IDC_SHOW_MANAGEMENT_PAGE) \
  E(kActionPasteAndGo, IDC_PASTE_AND_GO) \
  E(kActionShowFullUrls, IDC_SHOW_FULL_URLS) \
  E(kActionShowGoogleLensShortcut, IDC_SHOW_GOOGLE_LENS_SHORTCUT) \
  E(kActionShowAiModeOmniboxButton, IDC_SHOW_AI_MODE_OMNIBOX_BUTTON) \
  E(kActionRecordReplay) \
  E(kActionShowSearchTools, IDC_SHOW_SEARCH_TOOLS) \
  E(kActionCaretBrowsingToggle, IDC_CARET_BROWSING_TOGGLE) \
  E(kActionChromeTips, IDC_CHROME_TIPS) \
  E(kActionChromeWhatsNew, IDC_CHROME_WHATS_NEW) \
  E(kActionPerformance, IDC_PERFORMANCE) \
  E(kActionExtensionsSubmenuManageExtensions, \
    IDC_EXTENSIONS_SUBMENU_MANAGE_EXTENSIONS) \
  E(kActionExtensionsSubmenuVisitChromeWebStore, \
    IDC_EXTENSIONS_SUBMENU_VISIT_CHROME_WEB_STORE) \
  E(kActionReadingListMenuAddTab, IDC_READING_LIST_MENU_ADD_TAB) \
  E(kActionRecentTabsLoginForDeviceTabs, \
    IDC_RECENT_TABS_LOGIN_FOR_DEVICE_TABS) \
  E(kActionRecentTabsSeeDeviceTabs, \
    IDC_RECENT_TABS_SEE_DEVICE_TABS) \
  E(kActionOpenRecentTab, IDC_OPEN_RECENT_TAB) \
  E(kActionIndigo) \
  E(kActionAnchoredContextualCue) \
  E(kActionMultistepFilter) \
  E(kActionCheckSpellingWhileTyping, IDC_CHECK_SPELLING_WHILE_TYPING) \
  E(kActionSpellcheckAddToDictionary, IDC_SPELLCHECK_ADD_TO_DICTIONARY) \
  E(kActionSpellcheckMultiLingual, IDC_SPELLCHECK_MULTI_LINGUAL) \
  E(kActionSpellcheckRemoveFromDictionary, \
    IDC_SPELLCHECK_REMOVE_FROM_DICTIONARY) \
  /* Writing direction */ \
  E(kActionWritingDirectionLtr, IDC_WRITING_DIRECTION_LTR) \
  E(kActionWritingDirectionRtl, IDC_WRITING_DIRECTION_RTL) \
  E(kActionHideApp, IDC_HIDE_APP) \
  /* Link items. */ \
  E(kActionContentContextOpenLinkNewTab, IDC_CONTENT_CONTEXT_OPENLINKNEWTAB) \
  E(kActionContentContextOpenLinkNewWindow, \
    IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW) \
  E(kActionContentContextOpenLinkOffTheRecord, \
    IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD) \
  E(kActionContentContextSaveLinkAs, IDC_CONTENT_CONTEXT_SAVELINKAS) \
  E(kActionContentContextCopyLinkLocation, \
    IDC_CONTENT_CONTEXT_COPYLINKLOCATION) \
  E(kActionContentContextCopyLinkText, IDC_CONTENT_CONTEXT_COPYLINKTEXT) \
  E(kActionContentContextOpenLinkInProfile, \
    IDC_CONTENT_CONTEXT_OPENLINKINPROFILE) \
  E(kActionContentContextOpenLinkBookmarkApp, \
    IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP) \
  /* Image items. */ \
  E(kActionContentContextSaveImageAs, IDC_CONTENT_CONTEXT_SAVEIMAGEAS) \
  E(kActionContentContextCopyImageLocation, \
    IDC_CONTENT_CONTEXT_COPYIMAGELOCATION) \
  E(kActionContentContextCopyImage, IDC_CONTENT_CONTEXT_COPYIMAGE) \
  E(kActionContentContextOpenImageInNewTab, \
    IDC_CONTENT_CONTEXT_OPENIMAGENEWTAB) \
  E(kActionContentContextSearchWebForImage, \
    IDC_CONTENT_CONTEXT_SEARCHWEBFORIMAGE) \
  E(kActionContentContextOpenOriginalImageNewTab, \
    IDC_CONTENT_CONTEXT_OPEN_ORIGINAL_IMAGE_NEW_TAB) \
  E(kActionContentContextLoadImage, IDC_CONTENT_CONTEXT_LOAD_IMAGE) \
  E(kActionContentContextSearchLensForImage, \
    IDC_CONTENT_CONTEXT_SEARCHLENSFORIMAGE) \
  E(kActionContentContextGlicShareImage, \
    IDC_CONTENT_CONTEXT_GLICSHAREIMAGE) \
  E(kActionContentContextTranslateImageWithWeb) \
  E(kActionContentContextTranslateImageWithLens) \
  /* Audio/video items. */ \
  E(kActionContentContextSaveAvAs, IDC_CONTENT_CONTEXT_SAVEAVAS) \
  E(kActionContentContextCopyAvLocation, IDC_CONTENT_CONTEXT_COPYAVLOCATION) \
  E(kActionContentContextCopyVideoFrame, IDC_CONTENT_CONTEXT_COPYVIDEOFRAME) \
  E(kActionContentContextOpenAvNewTab, IDC_CONTENT_CONTEXT_OPENAVNEWTAB) \
  E(kActionContentContextPictureInPicture, \
    IDC_CONTENT_CONTEXT_PICTUREINPICTURE) \
  /* Media items. */ \
  E(kActionContentContextLoop, IDC_CONTENT_CONTEXT_LOOP) \
  E(kActionContentContextControls, IDC_CONTENT_CONTEXT_CONTROLS) \
  E(kActionContentContextRotateCw, IDC_CONTENT_CONTEXT_ROTATECW) \
  E(kActionContentContextRotateCcw, IDC_CONTENT_CONTEXT_ROTATECCW) \
  /* Edit items. */ \
  E(kActionContentContextCopy, IDC_CONTENT_CONTEXT_COPY) \
  E(kActionContentContextCut, IDC_CONTENT_CONTEXT_CUT) \
  E(kActionContentContextPaste, IDC_CONTENT_CONTEXT_PASTE) \
  E(kActionContentContextDelete, IDC_CONTENT_CONTEXT_DELETE) \
  E(kActionContentContextUndo, IDC_CONTENT_CONTEXT_UNDO) \
  E(kActionContentContextRedo, IDC_CONTENT_CONTEXT_REDO) \
  E(kActionContentContextSelectall, IDC_CONTENT_CONTEXT_SELECTALL) \
  E(kActionContentContextPasteAndMatchStyle, \
    IDC_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE) \
  E(kActionContentContextCopyLinkToText, IDC_CONTENT_CONTEXT_COPYLINKTOTEXT) \
  E(kActionContentContextReshareLinkToText, \
    IDC_CONTENT_CONTEXT_RESHARELINKTOTEXT) \
  E(kActionContentContextRemoveLinkToText, \
    IDC_CONTENT_CONTEXT_REMOVELINKTOTEXT) \
  /* Other items. */ \
  E(kActionContentContextTranslate, IDC_CONTENT_CONTEXT_TRANSLATE) \
  E(kActionContentContextInspectElement, IDC_CONTENT_CONTEXT_INSPECTELEMENT) \
  E(kActionContentContextLanguageSettings, \
    IDC_CONTENT_CONTEXT_LANGUAGE_SETTINGS) \
  E(kActionContentContextLookUp, IDC_CONTENT_CONTEXT_LOOK_UP) \
  E(kActionContentContextSpellingSuggestion, \
    IDC_CONTENT_CONTEXT_SPELLING_SUGGESTION) \
  E(kActionContentContextSpellingToggle, IDC_CONTENT_CONTEXT_SPELLING_TOGGLE) \
  E(kActionContentContextOpenInReadingMode, \
    IDC_CONTENT_CONTEXT_OPEN_IN_READING_MODE) \
  E(kActionContentContextListenToThisPage, \
    IDC_CONTENT_CONTEXT_LISTEN_TO_THIS_PAGE) \
  E(kActionContentContextSavePluginAs, IDC_CONTENT_CONTEXT_SAVEPLUGINAS) \
  E(kActionContentContextInspectBackgroundPage, \
    IDC_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE) \
  E(kActionContentContextReloadPackagedApp, \
    IDC_CONTENT_CONTEXT_RELOAD_PACKAGED_APP) \
  E(kActionContentContextRestartPackagedApp, \
    IDC_CONTENT_CONTEXT_RESTART_PACKAGED_APP) \
  E(kActionContentContextLensRegionSearch, \
    IDC_CONTENT_CONTEXT_LENS_REGION_SEARCH) \
  E(kActionAiMode) \
  E(kActionLensOverlayHomework) \
  E(kActionContentContextWebRegionSearch, \
    IDC_CONTENT_CONTEXT_WEB_REGION_SEARCH) \
  E(kActionContentContextGeneratePassword, \
    IDC_CONTENT_CONTEXT_GENERATEPASSWORD) \
  E(kActionContentContextExitFullscreen, IDC_CONTENT_CONTEXT_EXIT_FULLSCREEN) \
  E(kActionContentContextSaveToMemoryBanks, \
    IDC_CONTENT_CONTEXT_SAVE_TO_MEMORY_BANKS) \
  E(kActionContentContextShowAllSavedPasswords, \
    IDC_CONTENT_CONTEXT_SHOWALLSAVEDPASSWORDS) \
  E(kActionContentContextUsePasskeyFromAnotherDeviceTopLevel, \
    IDC_CONTENT_CONTEXT_USE_PASSKEY_FROM_ANOTHER_DEVICE) \
  E(kActionContentContextPartialTranslate, \
    IDC_CONTENT_CONTEXT_PARTIAL_TRANSLATE) \
  /* Frame items. */ \
  E(kActionContentContextReloadFrame, IDC_CONTENT_CONTEXT_RELOADFRAME) \
  E(kActionContentContextViewFrameSource, IDC_CONTENT_CONTEXT_VIEWFRAMESOURCE) \
  /* Search items. */ \
  E(kActionContentContextGoToUrl, IDC_CONTENT_CONTEXT_GOTOURL) \
  E(kActionContentContextSearchWebFor, IDC_CONTENT_CONTEXT_SEARCHWEBFOR) \
  E(kActionContentContextSearchWebForNewTab, \
    IDC_CONTENT_CONTEXT_SEARCHWEBFORNEWTAB) \
  /* Context menu items that provide fast access to input methods. */ \
  E(kActionContentContextEmoji, IDC_CONTENT_CONTEXT_EMOJI) \
  /* Context menu items in the bookmark bar */ \
  E(kActionBookmarkBarOpenAll, IDC_BOOKMARK_BAR_OPEN_ALL) \
  E(kActionBookmarkBarOpenAllNewWindow, IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW) \
  E(kActionBookmarkBarOpenAllIncognito, IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO) \
  E(kActionBookmarkBarOpenIncognito, IDC_BOOKMARK_BAR_OPEN_INCOGNITO) \
  E(kActionBookmarkBarOpenAllNewTabGroup, \
    IDC_BOOKMARK_BAR_OPEN_ALL_NEW_TAB_GROUP) \
  E(kActionBookmarkBarRenameFolder, IDC_BOOKMARK_BAR_RENAME_FOLDER) \
  E(kActionBookmarkBarEdit, IDC_BOOKMARK_BAR_EDIT) \
  E(kActionBookmarkBarRemove, IDC_BOOKMARK_BAR_REMOVE) \
  E(kActionBookmarkBarUndo, IDC_BOOKMARK_BAR_UNDO) \
  E(kActionBookmarkBarRedo, IDC_BOOKMARK_BAR_REDO) \
  E(kActionBookmarkBarAddNewBookmark, IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK) \
  E(kActionBookmarkBarNewFolder, IDC_BOOKMARK_BAR_NEW_FOLDER) \
  E(kActionBookmarkManager, IDC_BOOKMARK_MANAGER) \
  E(kActionBookmarkBarAlwaysShow, IDC_BOOKMARK_BAR_ALWAYS_SHOW) \
  E(kActionBookmarkBarShowAppsShortcut, IDC_BOOKMARK_BAR_SHOW_APPS_SHORTCUT) \
  E(kActionBookmarkBarShowManagedBookmarks, \
    IDC_BOOKMARK_BAR_SHOW_MANAGED_BOOKMARKS) \
  E(kActionBookmarkBarTrackPriceForShoppingBookmark, \
    IDC_BOOKMARK_BAR_TRACK_PRICE_FOR_SHOPPING_BOOKMARK) \
  E(kActionBookmarkBarUntrackPriceForShoppingBookmark, \
    IDC_BOOKMARK_BAR_UNTRACK_PRICE_FOR_SHOPPING_BOOKMARK) \
  E(kActionBookmarkBarAddToBookmarksBar, \
    IDC_BOOKMARK_BAR_ADD_TO_BOOKMARKS_BAR) \
  E(kActionBookmarkBarRemoveFromBookmarksBar, \
    IDC_BOOKMARK_BAR_REMOVE_FROM_BOOKMARKS_BAR) \
  /* Context menu items for Sharing */ \
  E(kActionContentContextSharingClickToCallSingleDevice, \
    IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE) \
  E(kActionContentContextSharingSharedClipboardSingleDevice, \
    IDC_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_SINGLE_DEVICE) \
  E(kActionContentContextSharingSharedClipboardMultipleDevices, \
    IDC_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_MULTIPLE_DEVICES) \
  E(kActionContentContextGenerateQrCode, IDC_CONTENT_CONTEXT_GENERATE_QR_CODE) \
  E(kActionContentContextSharingSubmenu, IDC_CONTENT_CONTEXT_SHARING_SUBMENU) \
  /* Context menu item to show the clipboard history menu */ \
  E(kActionContentPasteFromClipboard, IDC_CONTENT_PASTE_FROM_CLIPBOARD) \
  /* Context menu items in the status tray */ \
  E(kActionStatusTrayKeepChromeRunningInBackground, \
    IDC_STATUS_TRAY_KEEP_CHROME_RUNNING_IN_BACKGROUND) \
  /* Context menu items for media router */ \
  E(kActionMediaRouterAbout, IDC_MEDIA_ROUTER_ABOUT) \
  E(kActionMediaRouterHelp, IDC_MEDIA_ROUTER_HELP) \
  E(kActionMediaRouterLearnMore, IDC_MEDIA_ROUTER_LEARN_MORE) \
  E(kActionMediaRouterToggleMediaRemoting, \
    IDC_MEDIA_ROUTER_TOGGLE_MEDIA_REMOTING) \
  /* Context menu items for media toolbar button */ \
  E(kActionMediaToolbarContextShowOtherSessions, \
    IDC_MEDIA_TOOLBAR_CONTEXT_SHOW_OTHER_SESSIONS) \
  /* Protocol handler menu entries */ \
  E(kActionContentContextProtocolHandlerSettings, \
    IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_SETTINGS) \
  /* Start smart text selection actions */ \
  /* Accessibility labels */ \
  E(kActionContentContextAccessibilityLabelsToggle, \
    IDC_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_TOGGLE) \
  E(kActionContentContextAccessibilityLabelsToggleOnce, \
    IDC_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_TOGGLE_ONCE) \
  /* Tab Search */ \
  E(kActionTabSearchClose, IDC_TAB_SEARCH_CLOSE) \
  E(kActionTabSearchTogglePin, IDC_TAB_SEARCH_TOGGLE_PIN) \
  /* Views debug commands. */ \
  E(kActionDebugToggleTabletMode, IDC_DEBUG_TOGGLE_TABLET_MODE) \
  E(kActionDebugPrintViewTree, IDC_DEBUG_PRINT_VIEW_TREE) \
  E(kActionDebugPrintViewTreeDetails, IDC_DEBUG_PRINT_VIEW_TREE_DETAILS) \
  /* Autofill feedback. */ \
  E(kActionContentContextAutofillFeedback, \
    IDC_CONTENT_CONTEXT_AUTOFILL_FEEDBACK) \
  /* Autofill context menu commands */ \
  E(kActionContentContextAutofillImprovedSuggestions) \
  E(kActionContentContextAutofillFallbackPasswordsSelectPassword, \
    IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SELECT_PASSWORD) \
  E(kActionContentContextAutofillFallbackPasswordsImportPasswords, \
    IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_IMPORT_PASSWORDS) \
  E(kActionContentContextAutofillFallbackPasswordsSuggestPassword, \
    IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SUGGEST_PASSWORD) \
  E(kActionContentContextUsePasskeyFromAnotherDevice, \
    IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_USE_PASSKEY_FROM_ANOTHER_DEVICE) \
  /* Live Caption */ \
  E(kActionLiveCaption, IDC_LIVE_CAPTION) \
  /* Device API system tray icon */ \
  E(kActionDeviceSystemTrayIconFirst, IDC_DEVICE_SYSTEM_TRAY_ICON_FIRST) \
  /*Shows the Intent Picker bubble */ \
  E(kActionShowIntentPicker) \
  /*Shows the File System Access bubble */ \
  E(kActionShowFileSystemAccess) \
  /*Toolbar pinning*/ \
  E(kActionPinActionToToolbar) \
  E(kActionUnpinActionFromToolbar) \
  /*Commerce*/ \
  E(kActionCommercePriceInsights) \
  E(kActionCommerceDiscounts) \
  /*Vertical Tabs*/ \
  E(kActionToggleVerticalTabs, IDC_TOGGLE_VERTICAL_TABS) \
  E(kActionToggleCollapseVertical, IDC_TOGGLE_VERTICAL_TABS_COLLAPSE) \
  /*Projects Panel*/ \
  E(kActionToggleProjectsPanel) \
  /*Omnibox Context Menu*/       \
  E(kActionOmniboxContextAddImage, IDC_OMNIBOX_CONTEXT_ADD_IMAGE)\
  E(kActionOmniboxContextAddFile, IDC_OMNIBOX_CONTEXT_ADD_FILE)  \
  E(kActionOmniboxContextCreateImages, IDC_OMNIBOX_CONTEXT_CREATE_IMAGES)  \
  E(kActionOmniboxContextDeepResearch, IDC_OMNIBOX_CONTEXT_DEEP_RESEARCH)  \
  E(kActionOmniboxContextCanvas, IDC_OMNIBOX_CONTEXT_CANVAS)  \
  E(kActionOmniboxContextSetModelAuto, IDC_OMNIBOX_CONTEXT_SET_MODEL_AUTO)  \
  E(kActionOmniboxContextSetModelThinking, IDC_OMNIBOX_CONTEXT_SET_MODEL_THINKING)  \
  E(kActionOmniboxContextSetModelRegular, IDC_OMNIBOX_CONTEXT_SET_MODEL_REGULAR)  \
  E(kActionShowPaymentsChurnedUsersBubble) \

#if BUILDFLAG(IS_CHROMEOS)
#define CHROME_PLATFORM_SPECIFIC_ACTION_IDS \
  E(kToggleMultitaskMenu, IDC_TOGGLE_MULTITASK_MENU)
#elif BUILDFLAG(IS_LINUX)
#define CHROME_PLATFORM_SPECIFIC_ACTION_IDS \
  E(kUseSystemTitleBar, IDC_USE_SYSTEM_TITLE_BAR) \
  E(kRestoreWindow, IDC_RESTORE_WINDOW)
#elif BUILDFLAG(IS_WIN)
#define CHROME_PLATFORM_SPECIFIC_ACTION_IDS \
  E(kRestoreWindow, IDC_RESTORE_WINDOW)
#else
#define CHROME_PLATFORM_SPECIFIC_ACTION_IDS
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#define CHROME_GOOGLE_BRANDED_ACTION_IDS \
  /* Context menu items for media toolbar button */ \
  E(kActionMediaToolbarContextReportCastIssue, \
    IDC_MEDIA_TOOLBAR_CONTEXT_REPORT_CAST_ISSUE)
#else
#define CHROME_GOOGLE_BRANDED_ACTION_IDS
#endif // BUILDFLAG(GOOGLE_CHROME_BRANDING)

// TODO(crbug.com/40285337): Adding temporarily to unblock the side panel team.
// Should be reinserted into CHROME_COMMON_ACTION_IDS when general solution to
// add action id mappings is implemented.
#define SIDE_PANEL_ACTION_IDS \
  /* Side Panel items */ \
  E(kActionSidePanelShowAboutThisSite) \
  E(kActionSidePanelShowAssistant) \
  E(kActionSidePanelShowBookmarks, IDC_SHOW_BOOKMARK_SIDE_PANEL) \
  E(kActionSidePanelShowComments, IDC_SHOW_COMMENTS_SIDE_PANEL) \
  E(kActionSidePanelShowCustomizeChrome, IDC_SHOW_CUSTOMIZE_CHROME_SIDE_PANEL) \
  E(kActionSidePanelShowCustomizeChromeFooter) \
  E(kActionSidePanelShowCustomizeChromeToolbar) \
  E(kActionSidePanelShowFeed) \
  E(kActionSidePanelShowGlic) \
  E(kActionSidePanelShowContextualTasks) \
  E(kActionSidePanelShowHistoryCluster, IDC_SHOW_HISTORY_CLUSTERS_SIDE_PANEL) \
  E(kActionSidePanelShowHistory, IDC_SHOW_HISTORY_SIDE_PANEL) \
  E(kActionSidePanelShowLens) \
  E(kActionSidePanelShowLensOverlayResults, IDC_CONTENT_CONTEXT_LENS_OVERLAY) \
  E(kActionSidePanelShowReadAnything, IDC_SHOW_READING_MODE_SIDE_PANEL) \
  E(kActionSidePanelShowReadingList, IDC_READING_LIST_MENU_SHOW_UI) \
  E(kActionSidePanelShowSearchCompanion) \
  E(kActionSidePanelShowShoppingInsights) \
  E(kActionSidePanelShowSideSearch) \
  E(kActionSidePanelShowMerchantTrust) \
  E(kActionSidePanelShowTabsFromOtherDevices, \
    IDC_SHOW_TABS_FROM_OTHER_DEVICES_SIDE_PANEL)

#define TOOLBAR_PINNABLE_ACTION_IDS \
  E(kActionHome, IDC_HOME) \
  E(kActionForward, IDC_FORWARD) \
  E(kActionNewIncognitoWindow, IDC_NEW_INCOGNITO_WINDOW) \
  E(kActionSendSharedTabGroupFeedback, IDC_SEND_SHARED_TAB_GROUP_FEEDBACK) \
  E(kActionShowPasswordManager, IDC_SHOW_PASSWORD_MANAGER) \
  E(kActionShowPaymentMethods, IDC_SHOW_PAYMENT_METHODS) \
  E(kActionShowAddresses, IDC_SHOW_ADDRESSES) \
  E(kActionShowAddressesBubbleOrPage) \
  E(kActionShowDownloads, IDC_SHOW_DOWNLOADS) \
  E(kActionClearBrowsingData, IDC_CLEAR_BROWSING_DATA) \
  E(kActionPrint, IDC_PRINT) \
  E(kActionShowTranslate, IDC_SHOW_TRANSLATE) \
  E(kActionSendTabToSelf, IDC_SEND_TAB_TO_SELF) \
  E(kActionQrCodeGenerator, IDC_QRCODE_GENERATOR) \
  E(kActionRouteMedia, IDC_ROUTE_MEDIA) \
  E(kActionTaskManager, IDC_TASK_MANAGER) \
  E(kActionDevTools, IDC_DEV_TOOLS) \
  E(kActionShowChromeLabs, IDC_SHOW_CHROME_LABS) \
  E(kActionSaveCreditCardForPage, IDC_SAVE_CREDIT_CARD_FOR_PAGE) \
  E(kActionSaveIbanForPage, IDC_SAVE_IBAN_FOR_PAGE) \
  E(kActionShowPaymentsBubbleOrPage) \
  E(kActionShowPasswordsBubbleOrPage) \
  E(kActionManagePasswordsForPage, IDC_MANAGE_PASSWORDS_FOR_PAGE) \
  E(kActionCopyUrl, IDC_COPY_URL) \
  E(kActionTabGroupsMenu, kSavedTabGroupsMenuId) \
  E(kActionTabSearch, IDC_TAB_SEARCH) \
  E(kActionSplitTab, IDC_NEW_SPLIT_TAB) \
  E(kActionFederation) \
  E(kActionGlicContextualCueing) \
  E(kActionShowAiOverlayDialog) \
  E(kActionWebAuthnAmbientSignin) \
  E(kActionAutofillPayment) \

#define CHROME_ACTION_IDS \
    CHROME_COMMON_ACTION_IDS \
    CHROME_PLATFORM_SPECIFIC_ACTION_IDS \
    CHROME_GOOGLE_BRANDED_ACTION_IDS

#include "ui/actions/action_id_macros.inc"

enum ChromeActionIds : actions::ActionId {
  kChromeActionsStart = actions::kActionsEnd,

  CHROME_ACTION_IDS
  SIDE_PANEL_ACTION_IDS
  TOOLBAR_PINNABLE_ACTION_IDS

  kChromeActionsEnd,
};

// Note that this second include is not redundant. The second inclusion of the
// .inc file serves to undefine the macros the first inclusion defined.
#include "ui/actions/action_id_macros.inc"

// clang-format on

#endif  // CHROME_BROWSER_UI_ACTIONS_CHROME_ACTION_ID_H_
