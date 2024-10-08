// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ACTIONS_CHROME_ACTION_ID_H_
#define CHROME_BROWSER_UI_ACTIONS_CHROME_ACTION_ID_H_

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
  E(kActionAlwaysOnTop, IDC_ALWAYS_ON_TOP) \
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
  E(kActionWindowMenu, IDC_WINDOW_MENU) \
  E(kActionMinimizeWindow, IDC_MINIMIZE_WINDOW) \
  E(kActionMaximizeWindow, IDC_MAXIMIZE_WINDOW) \
  E(kActionAllWindowsFront, IDC_ALL_WINDOWS_FRONT) \
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
  E(kActionMigrateLocalCreditCardForPage, \
    IDC_MIGRATE_LOCAL_CREDIT_CARD_FOR_PAGE) \
  E(kActionFocusThisTab, IDC_FOCUS_THIS_TAB) \
  E(kActionWindowCloseTabsToRight, IDC_WINDOW_CLOSE_TABS_TO_RIGHT) \
  E(kActionWindowCloseOtherTabs, IDC_WINDOW_CLOSE_OTHER_TABS) \
  E(kActionNewTabToRight, IDC_NEW_TAB_TO_RIGHT) \
  E(kActionSaveAutofillAddress, IDC_SAVE_AUTOFILL_ADDRESS) \
  E(kActionOffersAndRewardsForPage, IDC_OFFERS_AND_REWARDS_FOR_PAGE) \
  E(kActionWebauthn, IDC_WEBAUTHN) \
  E(kActionSharingHub, IDC_SHARING_HUB) \
  E(kActionSharingHubMenu, IDC_SHARING_HUB_MENU) \
  E(kActionVirtualCardManualFallback, IDC_VIRTUAL_CARD_MANUAL_FALLBACK) \
  E(kActionSharingHubScreenshot, IDC_SHARING_HUB_SCREENSHOT) \
  E(kActionVirtualCardEnroll, IDC_VIRTUAL_CARD_ENROLL) \
  E(kActionFollow, IDC_FOLLOW) \
  E(kActionUnfollow, IDC_UNFOLLOW) \
  E(kActionAutofillMandatoryReauth, IDC_AUTOFILL_MANDATORY_REAUTH) \
  E(kActionProfileMenuInAppMenu, IDC_PROFILE_MENU_IN_APP_MENU) \
  E(kActionPasswordsAndAutofillMenu, IDC_PASSWORDS_AND_AUTOFILL_MENU) \
  /* Page-manipulation commands that target a specified tab, which may not */ \
  /* be the active one. */ \
  E(kActionMuteTargetSite, IDC_MUTE_TARGET_SITE) \
  E(kActionPinTargetTab, IDC_PIN_TARGET_TAB) \
  E(kActionGroupTargetTab, IDC_GROUP_TARGET_TAB) \
  E(kActionDuplicateTargetTab, IDC_DUPLICATE_TARGET_TAB) \
  /* Clipboard commands */ \
  E(kActionEditMenu, IDC_EDIT_MENU) \
  /* Find-in-page */ \
  E(kActionFind, IDC_FIND) \
  E(kActionFindNext, IDC_FIND_NEXT) \
  E(kActionFindPrevious, IDC_FIND_PREVIOUS) \
  E(kActionCloseFindOrStop, IDC_CLOSE_FIND_OR_STOP) \
  E(kActionFindMenu, IDC_FIND_MENU) \
  /* Find/Edit sub menu */ \
  E(kActionFindAndEditMenu, IDC_FIND_AND_EDIT_MENU) \
  /* Save/Share sub menu */ \
  E(kActionSaveAndShareMenu, IDC_SAVE_AND_SHARE_MENU) \
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
  /* Zoom */ \
  E(kActionZoomMenu, IDC_ZOOM_MENU) \
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
  E(kActionDeveloperMenu, IDC_DEVELOPER_MENU) \
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
  E(kActionBookmarksMenu, IDC_BOOKMARKS_MENU) \
  E(kActionExtensionErrors, IDC_EXTENSION_ERRORS) \
  E(kActionShowSettingsChangeFirst, IDC_SHOW_SETTINGS_CHANGE_FIRST) \
  E(kActionShowSettingsChangeLast, IDC_SHOW_SETTINGS_CHANGE_LAST) \
  E(kActionShowAvatarMenu, IDC_SHOW_AVATAR_MENU) \
  E(kActionExtensionInstallErrorFirst, IDC_EXTENSION_INSTALL_ERROR_FIRST) \
  E(kActionExtensionInstallErrorLast, IDC_EXTENSION_INSTALL_ERROR_LAST) \
  E(kActionToggleRequestTabletSite, IDC_TOGGLE_REQUEST_TABLET_SITE) \
  E(kActionDevToolsToggle, IDC_DEV_TOOLS_TOGGLE) \
  E(kActionRecentTabsMenu, IDC_RECENT_TABS_MENU) \
  E(kActionRecentTabsNoDeviceTabs, IDC_RECENT_TABS_NO_DEVICE_TABS) \
  E(kActionShowSettingsResetBubble, IDC_SHOW_SETTINGS_RESET_BUBBLE) \
  E(kActionHelpMenu, IDC_HELP_MENU) \
  E(kActionShowSrtBubble, IDC_SHOW_SRT_BUBBLE) \
  E(kActionElevatedRecoveryDialog, IDC_ELEVATED_RECOVERY_DIALOG) \
  E(kActionTakeScreenshot, IDC_TAKE_SCREENSHOT) \
  E(kActionMoreToolsMenu, IDC_MORE_TOOLS_MENU) \
  E(kActionToggleFullscreenToolbar, IDC_TOGGLE_FULLSCREEN_TOOLBAR) \
  E(kActionCustomizeTouchBar, IDC_CUSTOMIZE_TOUCH_BAR) \
  E(kActionShowBetaForum, IDC_SHOW_BETA_FORUM) \
  E(kActionToggleJavascriptAppleEvents, IDC_TOGGLE_JAVASCRIPT_APPLE_EVENTS) \
  E(kActionInstallPwa, IDC_INSTALL_PWA) \
  E(kActionShowManagementPage, IDC_SHOW_MANAGEMENT_PAGE) \
  E(kActionPasteAndGo, IDC_PASTE_AND_GO) \
  E(kActionShowSaveLocalCardSignInPromoIfApplicable, \
    IDC_SHOW_SAVE_LOCAL_CARD_SIGN_IN_PROMO_IF_APPLICABLE) \
  E(kActionCloseSignInPromo, IDC_CLOSE_SIGN_IN_PROMO) \
  E(kActionShowFullUrls, IDC_SHOW_FULL_URLS) \
  E(kActionShowGoogleLensShortcut, IDC_SHOW_GOOGLE_LENS_SHORTCUT) \
  E(kActionCaretBrowsingToggle, IDC_CARET_BROWSING_TOGGLE) \
  E(kActionChromeTips, IDC_CHROME_TIPS) \
  E(kActionChromeWhatsNew, IDC_CHROME_WHATS_NEW) \
  E(kActionPerformance, IDC_PERFORMANCE) \
  E(kActionExtensionsSubmenu, IDC_EXTENSIONS_SUBMENU) \
  E(kActionExtensionsSubmenuManageExtensions, \
    IDC_EXTENSIONS_SUBMENU_MANAGE_EXTENSIONS) \
  E(kActionExtensionsSubmenuVisitChromeWebStore, \
    IDC_EXTENSIONS_SUBMENU_VISIT_CHROME_WEB_STORE) \
  E(kActionReadingListMenu, IDC_READING_LIST_MENU) \
  E(kActionReadingListMenuAddTab, IDC_READING_LIST_MENU_ADD_TAB) \
  E(kActionRecentTabsLoginForDeviceTabs, \
    IDC_RECENT_TABS_LOGIN_FOR_DEVICE_TABS) \
  E(kActionOpenRecentTab, IDC_OPEN_RECENT_TAB) \
  /* Spell-check */ \
  /* Insert any additional suggestions before _LAST; these have to be */ \
  /* consecutive. */ \
  E(kActionSpellcheckSuggestion0, IDC_SPELLCHECK_SUGGESTION_0) \
  E(kActionSpellcheckSuggestion1, IDC_SPELLCHECK_SUGGESTION_1) \
  E(kActionSpellcheckSuggestion2, IDC_SPELLCHECK_SUGGESTION_2) \
  E(kActionSpellcheckSuggestion3, IDC_SPELLCHECK_SUGGESTION_3) \
  E(kActionSpellcheckSuggestion4, IDC_SPELLCHECK_SUGGESTION_4) \
  E(kActionSpellcheckSuggestionLast, IDC_SPELLCHECK_SUGGESTION_LAST) \
  E(kActionSpellcheckMenu, IDC_SPELLCHECK_MENU) \
  /* Language entries are inserted using autogenerated values between */ \
  /* [_FIRST, _LAST). */ \
  E(kActionSpellcheckLanguagesFirst, IDC_SPELLCHECK_LANGUAGES_FIRST) \
  E(kActionSpellcheckLanguagesLast, IDC_SPELLCHECK_LANGUAGES_LAST) \
  E(kActionCheckSpellingWhileTyping, IDC_CHECK_SPELLING_WHILE_TYPING) \
  E(kActionSpellpanelToggle, IDC_SPELLPANEL_TOGGLE) \
  E(kActionSpellcheckAddToDictionary, IDC_SPELLCHECK_ADD_TO_DICTIONARY) \
  E(kActionSpellcheckMultiLingual, IDC_SPELLCHECK_MULTI_LINGUAL) \
  /* Writing direction */ \
  E(kActionWritingDirectionMenu, IDC_WRITING_DIRECTION_MENU) \
  E(kActionWritingDirectionDefault, IDC_WRITING_DIRECTION_DEFAULT) \
  E(kActionWritingDirectionLtr, IDC_WRITING_DIRECTION_LTR) \
  E(kActionWritingDirectionRtl, IDC_WRITING_DIRECTION_RTL) \
  /* Translate */ \
  E(kActionTranslateOriginalLanguageBase, \
    IDC_TRANSLATE_ORIGINAL_LANGUAGE_BASE) \
  E(kActionTranslateTargetLanguageBase, IDC_TRANSLATE_TARGET_LANGUAGE_BASE) \
  /* Identifiers for platform-specific items. */ \
  /* Placed in a common file to help insure they never collide. */ \
  E(kActionViewMenu, IDC_VIEW_MENU) \
  E(kActionFileMenu, IDC_FILE_MENU) \
  E(kActionChromeMenu, IDC_CHROME_MENU) \
  E(kActionHideApp, IDC_HIDE_APP) \
  E(kActionHistoryMenu, IDC_HISTORY_MENU) \
  E(kActionTabMenu, IDC_TAB_MENU) \
  E(kActionProfileMainMenu, IDC_PROFILE_MAIN_MENU) \
  E(kActionInputMethodsMenu, IDC_INPUT_METHODS_MENU) \
  /* Range of command ids reserved for context menus added by web content */ \
  E(kActionContentContextCustomFirst, IDC_CONTENT_CONTEXT_CUSTOM_FIRST) \
  E(kActionContentContextCustomLast, IDC_CONTENT_CONTEXT_CUSTOM_LAST) \
  /* Range of command ids reserved for context menus added by extensions */ \
  E(kActionExtensionsContextCustomFirst, IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST) \
  E(kActionExtensionsContextCustomLast, IDC_EXTENSIONS_CONTEXT_CUSTOM_LAST) \
  /* Context menu items in the render view. */ \
  /* Link items. */ \
  E(kActionContentContextOpenLinkNewTab, IDC_CONTENT_CONTEXT_OPENLINKNEWTAB) \
  E(kActionContentContextOpenLinkNewWindow, \
    IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW) \
  E(kActionContentContextOpenLinkOffTheRecord, \
    IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD) \
  E(kActionContentContextSaveLinkAs, IDC_CONTENT_CONTEXT_SAVELINKAS) \
  E(kActionContentContextCopyLinkLocation, \
    IDC_CONTENT_CONTEXT_COPYLINKLOCATION) \
  E(kActionContentContextCopyEmailAddress, \
    IDC_CONTENT_CONTEXT_COPYEMAILADDRESS) \
  E(kActionContentContextOpenLinkWith, IDC_CONTENT_CONTEXT_OPENLINKWITH) \
  E(kActionContentContextCopyLinkText, IDC_CONTENT_CONTEXT_COPYLINKTEXT) \
  E(kActionContentContextOpenLinkInProfile, \
    IDC_CONTENT_CONTEXT_OPENLINKINPROFILE) \
  E(kActionContentContextOpenLinkBookmarkApp, \
    IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP) \
  E(kActionContentContextOpenLinkPreview, IDC_CONTENT_CONTEXT_OPENLINKPREVIEW) \
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
  E(kActionContentContextTranslateImageWithWeb, \
    IDC_CONTENT_CONTEXT_TRANSLATEIMAGEWITHWEB) \
  E(kActionContentContextTranslateImageWithLens, \
    IDC_CONTENT_CONTEXT_TRANSLATEIMAGEWITHLENS) \
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
  E(kActionContentContextViewPageInfo, IDC_CONTENT_CONTEXT_VIEWPAGEINFO) \
  E(kActionContentContextLanguageSettings, \
    IDC_CONTENT_CONTEXT_LANGUAGE_SETTINGS) \
  E(kActionContentContextLookUp, IDC_CONTENT_CONTEXT_LOOK_UP) \
  E(kActionContentContextNoSpellingSuggestions, \
    IDC_CONTENT_CONTEXT_NO_SPELLING_SUGGESTIONS) \
  E(kActionContentContextSpellingSuggestion, \
    IDC_CONTENT_CONTEXT_SPELLING_SUGGESTION) \
  E(kActionContentContextSpellingToggle, IDC_CONTENT_CONTEXT_SPELLING_TOGGLE) \
  E(kActionContentContextOpenInReadingMode, \
    IDC_CONTENT_CONTEXT_OPEN_IN_READING_MODE) \
  E(kActionContentContextSavePluginAs, IDC_CONTENT_CONTEXT_SAVEPLUGINAS) \
  E(kActionContentContextInspectBackgroundPage, \
    IDC_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE) \
  E(kActionContentContextReloadPackagedApp, \
    IDC_CONTENT_CONTEXT_RELOAD_PACKAGED_APP) \
  E(kActionContentContextRestartPackagedApp, \
    IDC_CONTENT_CONTEXT_RESTART_PACKAGED_APP) \
  E(kActionContentContextLensRegionSearch, \
    IDC_CONTENT_CONTEXT_LENS_REGION_SEARCH) \
  E(kActionContentContextWebRegionSearch, \
    IDC_CONTENT_CONTEXT_WEB_REGION_SEARCH) \
  E(kActionContentContextGeneratePassword, \
    IDC_CONTENT_CONTEXT_GENERATEPASSWORD) \
  E(kActionContentContextExitFullscreen, IDC_CONTENT_CONTEXT_EXIT_FULLSCREEN) \
  E(kActionContentContextShowAllSavedPasswords, \
    IDC_CONTENT_CONTEXT_SHOWALLSAVEDPASSWORDS) \
  E(kActionContentContextPartialTranslate, \
    IDC_CONTENT_CONTEXT_PARTIAL_TRANSLATE) \
  /* Frame items. */ \
  E(kActionContentContextReloadFrame, IDC_CONTENT_CONTEXT_RELOADFRAME) \
  E(kActionContentContextViewFrameSource, IDC_CONTENT_CONTEXT_VIEWFRAMESOURCE) \
  E(kActionContentContextViewFrameInfo, IDC_CONTENT_CONTEXT_VIEWFRAMEINFO) \
  /* User Notes. */ \
  E(kActionContentContextAddANote, IDC_CONTENT_CONTEXT_ADD_A_NOTE) \
  /* Search items. */ \
  E(kActionContentContextGoToUrl, IDC_CONTENT_CONTEXT_GOTOURL) \
  E(kActionContentContextSearchWebFor, IDC_CONTENT_CONTEXT_SEARCHWEBFOR) \
  E(kActionContentContextSearchWebForNewTab, \
    IDC_CONTENT_CONTEXT_SEARCHWEBFORNEWTAB) \
  /* Open with items. */ \
  E(kActionContentContextOpenWith1, IDC_CONTENT_CONTEXT_OPEN_WITH1) \
  E(kActionContentContextOpenWith2, IDC_CONTENT_CONTEXT_OPEN_WITH2) \
  E(kActionContentContextOpenWith3, IDC_CONTENT_CONTEXT_OPEN_WITH3) \
  E(kActionContentContextOpenWith4, IDC_CONTENT_CONTEXT_OPEN_WITH4) \
  E(kActionContentContextOpenWith5, IDC_CONTENT_CONTEXT_OPEN_WITH5) \
  E(kActionContentContextOpenWith6, IDC_CONTENT_CONTEXT_OPEN_WITH6) \
  E(kActionContentContextOpenWith7, IDC_CONTENT_CONTEXT_OPEN_WITH7) \
  E(kActionContentContextOpenWith8, IDC_CONTENT_CONTEXT_OPEN_WITH8) \
  E(kActionContentContextOpenWith9, IDC_CONTENT_CONTEXT_OPEN_WITH9) \
  E(kActionContentContextOpenWith10, IDC_CONTENT_CONTEXT_OPEN_WITH10) \
  E(kActionContentContextOpenWith11, IDC_CONTENT_CONTEXT_OPEN_WITH11) \
  E(kActionContentContextOpenWith12, IDC_CONTENT_CONTEXT_OPEN_WITH12) \
  E(kActionContentContextOpenWith13, IDC_CONTENT_CONTEXT_OPEN_WITH13) \
  E(kActionContentContextOpenWith14, IDC_CONTENT_CONTEXT_OPEN_WITH14) \
  E(kActionContentContextOpenWithLast, IDC_CONTENT_CONTEXT_OPEN_WITH_LAST) \
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
  E(kActionBookmarkBarShowReadingList, IDC_BOOKMARK_BAR_SHOW_READING_LIST) \
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
  E(kActionContentContextSharingClickToCallMultipleDevices, \
    IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_MULTIPLE_DEVICES) \
  E(kActionContentContextSharingSharedClipboardSingleDevice, \
    IDC_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_SINGLE_DEVICE) \
  E(kActionContentContextSharingSharedClipboardMultipleDevices, \
    IDC_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_MULTIPLE_DEVICES) \
  E(kActionContentContextGenerateQrCode, IDC_CONTENT_CONTEXT_GENERATE_QR_CODE) \
  E(kActionContentContextSharingSubmenu, IDC_CONTENT_CONTEXT_SHARING_SUBMENU) \
  /* Context menu item to show the clipboard history menu */ \
  E(kActionContentClipboardHistoryMenu, IDC_CONTENT_CLIPBOARD_HISTORY_MENU) \
  E(kActionContentPasteFromClipboard, IDC_CONTENT_PASTE_FROM_CLIPBOARD) \
  /* Context menu items in the status tray */ \
  E(kActionStatusTrayKeepChromeRunningInBackground, \
    IDC_STATUS_TRAY_KEEP_CHROME_RUNNING_IN_BACKGROUND) \
  /* Context menu items for media router */ \
  E(kActionMediaRouterAbout, IDC_MEDIA_ROUTER_ABOUT) \
  E(kActionMediaRouterHelp, IDC_MEDIA_ROUTER_HELP) \
  E(kActionMediaRouterLearnMore, IDC_MEDIA_ROUTER_LEARN_MORE) \
  E(kActionMediaRouterAlwaysShowToolbarAction, \
    IDC_MEDIA_ROUTER_ALWAYS_SHOW_TOOLBAR_ACTION) \
  E(kActionMediaRouterShownByPolicy, IDC_MEDIA_ROUTER_SHOWN_BY_POLICY) \
  E(kActionMediaRouterShowInToolbar, IDC_MEDIA_ROUTER_SHOW_IN_TOOLBAR) \
  E(kActionMediaRouterToggleMediaRemoting, \
    IDC_MEDIA_ROUTER_TOGGLE_MEDIA_REMOTING) \
  /* Context menu items for media toolbar button */ \
  E(kActionMediaToolbarContextShowOtherSessions, \
    IDC_MEDIA_TOOLBAR_CONTEXT_SHOW_OTHER_SESSIONS) \
  /* Context menu items for media stream status tray */ \
  E(kActionMediaStreamDeviceStatusTray, IDC_MEDIA_STREAM_DEVICE_STATUS_TRAY) \
  E(kActionMediaContextMediaStreamCaptureListFirst, \
    IDC_MEDIA_CONTEXT_MEDIA_STREAM_CAPTURE_LIST_FIRST) \
  E(kActionMediaContextMediaStreamCaptureListLast, \
    IDC_MEDIA_CONTEXT_MEDIA_STREAM_CAPTURE_LIST_LAST) \
  E(kActionMediaStreamDeviceAlwaysAllow, IDC_MEDIA_STREAM_DEVICE_ALWAYS_ALLOW) \
  /* Protocol handler menu entries */ \
  E(kActionContentContextProtocolHandlerFirst, \
    IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_FIRST) \
  E(kActionContentContextProtocolHandlerLast, \
    IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_LAST) \
  E(kActionContentContextProtocolHandlerSettings, \
    IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_SETTINGS) \
  /* Open link in profile entries */ \
  E(kActionOpenLinkInProfileFirst, IDC_OPEN_LINK_IN_PROFILE_FIRST) \
  E(kActionOpenLinkInProfileLast, IDC_OPEN_LINK_IN_PROFILE_LAST) \
  /* Start smart text selection actions */ \
  E(kActionContentContextStartSmartSelectionAction1, \
    IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION1) \
  E(kActionContentContextStartSmartSelectionAction2, \
    IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION2) \
  E(kActionContentContextStartSmartSelectionAction3, \
    IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION3) \
  E(kActionContentContextStartSmartSelectionAction4, \
    IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION4) \
  E(kActionContentContextStartSmartSelectionAction5, \
    IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION5) \
  E(kActionContentContextStartSmartSelectionActionLast, \
    IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION_LAST) \
  /* Accessibility labels */ \
  E(kActionContentContextAccessibilityLabelsToggle, \
    IDC_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_TOGGLE) \
  E(kActionContentContextAccessibilityLabels, \
    IDC_CONTENT_CONTEXT_ACCESSIBILITY_LABELS) \
  E(kActionContentContextAccessibilityLabelsToggleOnce, \
    IDC_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_TOGGLE_ONCE) \
  /* Tab Search */ \
  E(kActionTabSearch, IDC_TAB_SEARCH) \
  E(kActionTabSearchClose, IDC_TAB_SEARCH_CLOSE) \
  /* Views debug commands. */ \
  E(kActionDebugToggleTabletMode, IDC_DEBUG_TOGGLE_TABLET_MODE) \
  E(kActionDebugPrintViewTree, IDC_DEBUG_PRINT_VIEW_TREE) \
  E(kActionDebugPrintViewTreeDetails, IDC_DEBUG_PRINT_VIEW_TREE_DETAILS) \
  /* Autofill feedback. */ \
  E(kActionContentContextAutofillFeedback, \
    IDC_CONTENT_CONTEXT_AUTOFILL_FEEDBACK) \
  /* Autofill context menu commands */ \
  E(kActionContentContextAutofillImprovedSuggestions, \
    IDC_CONTENT_CONTEXT_AUTOFILL_PREDICTION_IMPROVEMENTS) \
  E(kActionContentContextAutofillFallbackAddress, \
    IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_ADDRESS) \
  E(kActionContentContextAutofillFallbackPayments, \
    IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PAYMENTS) \
  E(kActionContentContextAutofillFallbackPassowords, \
    IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS) \
  E(kActionContentContextAutofillFallbackPasswordsSelectPassword, \
    IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SELECT_PASSWORD) \
  E(kActionContentContextAutofillFallbackPasswordsImportPasswords, \
    IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_IMPORT_PASSWORDS) \
  E(kActionContentContextAutofillFallbackPasswordsSuggestPassword, \
    IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SUGGEST_PASSWORD) \
  E(kActionContentContextAutofillFallbackPasswordsNoSavedPasswords, \
    IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_NO_SAVED_PASSWORDS) \
  E(kActionContentContextUsePasskeyFromAnotherDevice, \
    IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_USE_PASSKEY_FROM_ANOTHER_DEVICE) \
  /* Live Caption */ \
  E(kActionLiveCaption, IDC_LIVE_CAPTION) \
  /* Device API system tray icon */ \
  E(kActionDeviceSystemTrayIconFirst, IDC_DEVICE_SYSTEM_TRAY_ICON_FIRST) \
  E(kActionDeviceSystemTrayIconLast, IDC_DEVICE_SYSTEM_TRAY_ICON_LAST)

#if BUILDFLAG(IS_CHROMEOS)
#define CHROME_PLATFORM_SPECIFIC_ACTION_IDS \
  E(kToggleMultitaskMenu, IDC_TOGGLE_MULTITASK_MENU)
#elif BUILDFLAG(IS_CHROMEOS_ASH)
#define CHROME_PLATFORM_SPECIFIC_ACTION_IDS \
  /* Move window to other user commands */ \
  E(kActionVisitDesktopOfLruUser2, IDC_VISIT_DESKTOP_OF_LRU_USER_2) \
  E(kActionVisitDesktopOfLruUser3, IDC_VISIT_DESKTOP_OF_LRU_USER_3) \
  E(kActionVisitDesktopOfLruUser4, IDC_VISIT_DESKTOP_OF_LRU_USER_4) \
  E(kActionVisitDesktopOfLruUser5, IDC_VISIT_DESKTOP_OF_LRU_USER_5) \
  E(kActionVisitDesktopOfLruUserNext, IDC_VISIT_DESKTOP_OF_LRU_USER_NEXT) \
  E(kActionVisitDesktopOfLruUserLast, IDC_VISIT_DESKTOP_OF_LRU_USER_LAST) \
  /* Quick Answers context menu items. */ \
  E(kActionContentContextQuickAnswersInlineAnswer, \
    IDC_CONTENT_CONTEXT_QUICK_ANSWERS_INLINE_ANSWER) \
  E(kActionContentContextQuickAnswersInlineQuery, \
    IDC_CONTENT_CONTEXT_QUICK_ANSWERS_INLINE_QUERY)
#elif BUILDFLAG(IS_LINUX)
#define CHROME_PLATFORM_SPECIFIC_ACTION_IDS \
  E(kUseSystemTitleBar, IDC_USE_SYSTEM_TITLE_BAR) \
  E(kRestoreWindow, IDC_RESTORE_WINDOW)
// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#define CHROME_PLATFORM_SPECIFIC_ACTION_IDS \
  E(kRestoreWindow, IDC_RESTORE_WINDOW)
#else
#define CHROME_PLATFORM_SPECIFIC_ACTION_IDS
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

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
  E(kActionSidePanelShowCustomizeChrome) \
  E(kActionSidePanelShowFeed) \
  E(kActionSidePanelShowHistoryCluster) \
  E(kActionSidePanelShowLens) \
  E(kActionSidePanelShowLensOverlayResults, IDC_CONTENT_CONTEXT_LENS_OVERLAY) \
  E(kActionSidePanelShowReadAnything) \
  E(kActionSidePanelShowReadingList, IDC_READING_LIST_MENU_SHOW_UI) \
  E(kActionSidePanelShowSearchCompanion, IDC_SHOW_SEARCH_COMPANION) \
  E(kActionSidePanelShowShoppingInsights) \
  E(kActionSidePanelShowSideSearch) \
  E(kActionSidePanelShowUserNote) \

#define TOOLBAR_PINNABLE_ACTION_IDS \
  E(kActionHome, IDC_HOME) \
  E(kActionForward, IDC_FORWARD) \
  E(kActionNewIncognitoWindow, IDC_NEW_INCOGNITO_WINDOW) \
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
