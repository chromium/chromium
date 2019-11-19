// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_CHROME_COMMAND_IDS_H_
#define CHROME_APP_CHROME_COMMAND_IDS_H_

// This file lists all the command IDs understood by e.g. the browser.
// It is used by Windows RC files, Mac NIB files, and other platforms too.

// clang-format off

// Values below IDC_MinimumLabelValue are reserved for dynamic menu items.
#define IDC_MinimumLabelValue           4000

// NOTE: Within each of the following sections, the IDs are ordered roughly by
// how they appear in the GUI/menus (left to right, top to bottom, etc.).

// Navigation commands
// TODO: Reorder to be in visible order; collapse holes
#define IDC_BACK                        33000
#define IDC_FORWARD                     33001
#define IDC_RELOAD                      33002
#define IDC_HOME                        33003
#define IDC_OPEN_CURRENT_URL            33004
#define IDC_STOP                        33006
#define IDC_RELOAD_BYPASSING_CACHE      33007
#define IDC_RELOAD_CLEARING_CACHE       33009

// Window management commands
#define IDC_NEW_WINDOW                  34000
#define IDC_NEW_INCOGNITO_WINDOW        34001
#define IDC_PIN_TO_START_SCREEN         34005
#define IDC_CLOSE_WINDOW                34012
#define IDC_ALWAYS_ON_TOP               34013
#define IDC_NEW_TAB                     34014
#define IDC_CLOSE_TAB                   34015
#define IDC_SELECT_NEXT_TAB             34016
#define IDC_SELECT_PREVIOUS_TAB         34017
#define IDC_SELECT_TAB_0                34018
#define IDC_SELECT_TAB_1                34019
#define IDC_SELECT_TAB_2                34020
#define IDC_SELECT_TAB_3                34021
#define IDC_SELECT_TAB_4                34022
#define IDC_SELECT_TAB_5                34023
#define IDC_SELECT_TAB_6                34024
#define IDC_SELECT_TAB_7                34025
#define IDC_SELECT_LAST_TAB             34026
#define IDC_DUPLICATE_TAB               34027
#define IDC_RESTORE_TAB                 34028
#define IDC_SHOW_AS_TAB                 34029
#define IDC_FULLSCREEN                  34030
#define IDC_EXIT                        34031
#define IDC_MOVE_TAB_NEXT               34032
#define IDC_MOVE_TAB_PREVIOUS           34033
#define IDC_SEARCH                      34035
#define IDC_DEBUG_FRAME_TOGGLE          34038
#define IDC_WINDOW_MENU                 34045
#define IDC_MINIMIZE_WINDOW             34046
#define IDC_MAXIMIZE_WINDOW             34047
#define IDC_ALL_WINDOWS_FRONT           34048
#define IDC_VISIT_DESKTOP_OF_LRU_USER_2 34049
#define IDC_VISIT_DESKTOP_OF_LRU_USER_3 34050

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#define IDC_USE_SYSTEM_TITLE_BAR        34051
#define IDC_RESTORE_WINDOW              34052
#endif

#define IDC_OPEN_IN_PWA_WINDOW          34053

// Web app window commands
#define IDC_COPY_URL                    34060
#define IDC_OPEN_IN_CHROME              34061
#define IDC_SITE_SETTINGS               34062
#define IDC_WEB_APP_MENU_APP_INFO    34063

#if defined(OS_CHROMEOS)
// Terminal system app commands
#define IDC_TERMINAL_SPLIT_HORIZONTAL   34070
#define IDC_TERMINAL_SPLIT_VERTICAL     34071
#endif

// Page-related commands
#define IDC_BOOKMARK_THIS_TAB           35000
#define IDC_BOOKMARK_ALL_TABS           35001
#define IDC_VIEW_SOURCE                 35002
#define IDC_PRINT                       35003
#define IDC_SAVE_PAGE                   35004
#define IDC_EMAIL_PAGE_LOCATION         35006
#define IDC_BASIC_PRINT                 35007
#define IDC_SAVE_CREDIT_CARD_FOR_PAGE   35008
#define IDC_TRANSLATE_PAGE              35009
#define IDC_MANAGE_PASSWORDS_FOR_PAGE   35010
#define IDC_ROUTE_MEDIA                 35011
#define IDC_WINDOW_MUTE_SITE            35012
#define IDC_WINDOW_PIN_TAB              35013
#define IDC_MIGRATE_LOCAL_CREDIT_CARD_FOR_PAGE 35014
#define IDC_SEND_TAB_TO_SELF            35015
#define IDC_FOCUS_THIS_TAB              35016
#define IDC_CONTENT_LINK_SEND_TAB_TO_SELF 35017
#define IDC_SEND_TAB_TO_SELF_SINGLE_TARGET  35018
#define IDC_CONTENT_LINK_SEND_TAB_TO_SELF_SINGLE_TARGET  35019
#define IDC_QRCODE_GENERATOR            35020
#define IDC_WINDOW_CLOSE_TABS_TO_RIGHT  35021
#define IDC_WINDOW_CLOSE_OTHER_TABS     35022

// Page-manipulation commands that target a specified tab, which may not be the
// active one.
#define IDC_MUTE_TARGET_SITE            35050
#define IDC_PIN_TARGET_TAB              35051
#define IDC_DUPLICATE_TARGET_TAB        35052

// Clipboard commands
#define IDC_CUT                         36000
#define IDC_COPY                        36001
#define IDC_PASTE                       36003
#define IDC_EDIT_MENU                   36004

// Find-in-page
#define IDC_FIND                        37000
#define IDC_FIND_NEXT                   37001
#define IDC_FIND_PREVIOUS               37002
#define IDC_CLOSE_FIND_OR_STOP          37003
#define IDC_FIND_MENU                   37100

// Zoom
#define IDC_ZOOM_MENU                   38000
#define IDC_ZOOM_PLUS                   38001
#define IDC_ZOOM_NORMAL                 38002
#define IDC_ZOOM_MINUS                  38003
#define IDC_ZOOM_PERCENT_DISPLAY        38004

// Focus various bits of UI
#define IDC_FOCUS_TOOLBAR               39000
#define IDC_FOCUS_LOCATION              39001
#define IDC_FOCUS_SEARCH                39002
#define IDC_FOCUS_MENU_BAR              39003
#define IDC_FOCUS_NEXT_PANE             39004
#define IDC_FOCUS_PREVIOUS_PANE         39005
#define IDC_FOCUS_BOOKMARKS             39006
#define IDC_FOCUS_INACTIVE_POPUP_FOR_ACCESSIBILITY 39007

// Show various bits of UI
#define IDC_OPEN_FILE                   40000
#define IDC_CREATE_SHORTCUT             40002
#define IDC_DEVELOPER_MENU              40003
#define IDC_DEV_TOOLS                   40004
#define IDC_DEV_TOOLS_CONSOLE           40005
#define IDC_TASK_MANAGER                40006
#define IDC_DEV_TOOLS_DEVICES           40007
#define IDC_FEEDBACK                    40008
#define IDC_SHOW_BOOKMARK_BAR           40009
#define IDC_SHOW_HISTORY                40010
#define IDC_SHOW_BOOKMARK_MANAGER       40011
#define IDC_SHOW_DOWNLOADS              40012
#define IDC_CLEAR_BROWSING_DATA         40013
#define IDC_IMPORT_SETTINGS             40014
#define IDC_OPTIONS                     40015
#define IDC_EDIT_SEARCH_ENGINES         40016
#define IDC_VIEW_PASSWORDS              40017
#define IDC_ABOUT                       40018
#define IDC_HELP_PAGE_VIA_KEYBOARD      40019
#define IDC_HELP_PAGE_VIA_MENU          40020
#define IDC_SHOW_APP_MENU               40021
#define IDC_MANAGE_EXTENSIONS           40022
#define IDC_DEV_TOOLS_INSPECT           40023
#define IDC_UPGRADE_DIALOG              40024
#define IDC_PROFILING_ENABLED           40028
#define IDC_BOOKMARKS_MENU              40029
#define IDC_SHOW_SIGNIN                 40030
#define IDC_EXTENSION_ERRORS            40031
#define IDC_SHOW_SIGNIN_ERROR           40032
#define IDC_SHOW_SETTINGS_CHANGE_FIRST  40033
#define IDC_SHOW_SETTINGS_CHANGE_LAST   40133
#define IDC_SHOW_AVATAR_MENU            40134
#define IDC_EXTENSION_INSTALL_ERROR_FIRST 40135
#define IDC_EXTENSION_INSTALL_ERROR_LAST 40235
#define IDC_TOGGLE_REQUEST_TABLET_SITE  40236
#define IDC_DEV_TOOLS_TOGGLE            40237
#define IDC_RECENT_TABS_MENU            40239
#define IDC_RECENT_TABS_NO_DEVICE_TABS  40240
#define IDC_SHOW_SETTINGS_RESET_BUBBLE  40241
#define IDC_SHOW_SYNC_ERROR             40242
#define IDC_DISTILL_PAGE                40243
#define IDC_HELP_MENU                   40244
#define IDC_EXTENSIONS_OVERFLOW_MENU    40245
#define IDC_SHOW_SRT_BUBBLE             40246
#define IDC_ELEVATED_RECOVERY_DIALOG    40247
#define IDC_TAKE_SCREENSHOT             40248
#define IDC_MORE_TOOLS_MENU             40249
#define IDC_TOGGLE_FULLSCREEN_TOOLBAR   40250
#define IDC_CUSTOMIZE_TOUCH_BAR         40251
#define IDC_SHOW_BETA_FORUM             40252
#define IDC_TOGGLE_JAVASCRIPT_APPLE_EVENTS 40253
#define IDC_INSTALL_PWA                 40254
#define IDC_SHOW_MANAGEMENT_PAGE             40255
#define IDC_PASTE_AND_GO                40256
#define IDC_SHOW_SAVE_LOCAL_CARD_SIGN_IN_PROMO_IF_APPLICABLE 40257
#define IDC_CLOSE_SIGN_IN_PROMO        40258

// Spell-check
// Insert any additional suggestions before _LAST; these have to be consecutive.
#define IDC_SPELLCHECK_SUGGESTION_0     41000
#define IDC_SPELLCHECK_SUGGESTION_1     41001
#define IDC_SPELLCHECK_SUGGESTION_2     41002
#define IDC_SPELLCHECK_SUGGESTION_3     41003
#define IDC_SPELLCHECK_SUGGESTION_4     41004
#define IDC_SPELLCHECK_SUGGESTION_LAST  IDC_SPELLCHECK_SUGGESTION_4
#define IDC_SPELLCHECK_MENU             41005
// Language entries are inserted using autogenerated values between
// [_FIRST, _LAST).
#define IDC_SPELLCHECK_LANGUAGES_FIRST  41006
#define IDC_SPELLCHECK_LANGUAGES_LAST   41106
#define IDC_CHECK_SPELLING_WHILE_TYPING 41107
#define IDC_SPELLPANEL_TOGGLE           41109
#define IDC_SPELLCHECK_ADD_TO_DICTIONARY 41110
#define IDC_SPELLCHECK_MULTI_LINGUAL    41111

// Writing direction
#define IDC_WRITING_DIRECTION_MENU       41120
#define IDC_WRITING_DIRECTION_DEFAULT    41121
#define IDC_WRITING_DIRECTION_LTR        41122
#define IDC_WRITING_DIRECTION_RTL        41123

// Translate
#define IDC_TRANSLATE_ORIGINAL_LANGUAGE_BASE 42100
#define IDC_TRANSLATE_TARGET_LANGUAGE_BASE   42400

// Identifiers for platform-specific items.
// Placed in a common file to help insure they never collide.
#define IDC_VIEW_MENU                   44000     // OSX only
#define IDC_FILE_MENU                   44001     // OSX only
#define IDC_CHROME_MENU                 44002     // OSX only
#define IDC_HIDE_APP                    44003     // OSX only
#define IDC_HISTORY_MENU                46000     // OSX only
#define IDC_TAB_MENU                    46001     // OSX only
#define IDC_PROFILE_MAIN_MENU           46100     // OSX only
#define IDC_INPUT_METHODS_MENU          46300     // Linux only

// The range of command ids reserved for context menus added by web content.
#define IDC_CONTENT_CONTEXT_CUSTOM_FIRST 47000
#define IDC_CONTENT_CONTEXT_CUSTOM_LAST  48000

// The range of command ids reserved for context menus added by extensions.
#define IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST 49000
#define IDC_EXTENSIONS_CONTEXT_CUSTOM_LAST 50000

// Context menu items in the render view.
// Link items.
#define IDC_CONTENT_CONTEXT_OPENLINKNEWTAB 50100
#define IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW 50101
#define IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD 50102
#define IDC_CONTENT_CONTEXT_SAVELINKAS 50103
#define IDC_CONTENT_CONTEXT_COPYLINKLOCATION 50104
#define IDC_CONTENT_CONTEXT_COPYEMAILADDRESS 50105
#define IDC_CONTENT_CONTEXT_OPENLINKWITH 50106
#define IDC_CONTENT_CONTEXT_COPYLINKTEXT 50107
#define IDC_CONTENT_CONTEXT_OPENLINKINPROFILE 50108
#define IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP 50109
// Image items.
#define IDC_CONTENT_CONTEXT_SAVEIMAGEAS 50110
#define IDC_CONTENT_CONTEXT_COPYIMAGELOCATION 50111
#define IDC_CONTENT_CONTEXT_COPYIMAGE 50112
#define IDC_CONTENT_CONTEXT_OPENIMAGENEWTAB 50113
#define IDC_CONTENT_CONTEXT_SEARCHWEBFORIMAGE 50114
#define IDC_CONTENT_CONTEXT_OPEN_ORIGINAL_IMAGE_NEW_TAB 50115
#define IDC_CONTENT_CONTEXT_LOAD_IMAGE 50116
// Audio/video items.
#define IDC_CONTENT_CONTEXT_SAVEAVAS 50120
#define IDC_CONTENT_CONTEXT_COPYAVLOCATION 50121
#define IDC_CONTENT_CONTEXT_OPENAVNEWTAB 50122
#define IDC_CONTENT_CONTEXT_PICTUREINPICTURE 50123
// Media items.
#define IDC_CONTENT_CONTEXT_PLAYPAUSE 50130
#define IDC_CONTENT_CONTEXT_MUTE 50131
#define IDC_CONTENT_CONTEXT_LOOP 50132
#define IDC_CONTENT_CONTEXT_CONTROLS 50133
#define IDC_CONTENT_CONTEXT_ROTATECW 50134
#define IDC_CONTENT_CONTEXT_ROTATECCW 50135
// Edit items.
#define IDC_CONTENT_CONTEXT_COPY 50140
#define IDC_CONTENT_CONTEXT_CUT 50141
#define IDC_CONTENT_CONTEXT_PASTE 50142
#define IDC_CONTENT_CONTEXT_DELETE 50143
#define IDC_CONTENT_CONTEXT_UNDO 50144
#define IDC_CONTENT_CONTEXT_REDO 50145
#define IDC_CONTENT_CONTEXT_SELECTALL 50146
#define IDC_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE 50147
// Other items.
#define IDC_CONTENT_CONTEXT_TRANSLATE 50150
#define IDC_CONTENT_CONTEXT_INSPECTELEMENT 50151
#define IDC_CONTENT_CONTEXT_VIEWPAGEINFO 50152
#define IDC_CONTENT_CONTEXT_LANGUAGE_SETTINGS 50153
#define IDC_CONTENT_CONTEXT_LOOK_UP 50154
#define IDC_CONTENT_CONTEXT_NO_SPELLING_SUGGESTIONS 50155
#define IDC_CONTENT_CONTEXT_SPELLING_SUGGESTION 50156
#define IDC_CONTENT_CONTEXT_SPELLING_TOGGLE 50157
#define IDC_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE 50161
#define IDC_CONTENT_CONTEXT_RELOAD_PACKAGED_APP 50162
#define IDC_CONTENT_CONTEXT_RESTART_PACKAGED_APP 50163
// A gap here. Feel free to insert new IDs.
#define IDC_CONTENT_CONTEXT_GENERATEPASSWORD 50166
#define IDC_CONTENT_CONTEXT_EXIT_FULLSCREEN 50167
#define IDC_CONTENT_CONTEXT_SHOWALLSAVEDPASSWORDS 50168
// Frame items.
#define IDC_CONTENT_CONTEXT_RELOADFRAME 50170
#define IDC_CONTENT_CONTEXT_VIEWFRAMESOURCE 50171
#define IDC_CONTENT_CONTEXT_VIEWFRAMEINFO 50172
// Search items.
#define IDC_CONTENT_CONTEXT_GOTOURL 50180
#define IDC_CONTENT_CONTEXT_SEARCHWEBFOR 50181
// Open with items.
#define IDC_CONTENT_CONTEXT_OPEN_WITH1 50190
#define IDC_CONTENT_CONTEXT_OPEN_WITH2 50191
#define IDC_CONTENT_CONTEXT_OPEN_WITH3 50192
#define IDC_CONTENT_CONTEXT_OPEN_WITH4 50193
#define IDC_CONTENT_CONTEXT_OPEN_WITH5 50194
#define IDC_CONTENT_CONTEXT_OPEN_WITH6 50195
#define IDC_CONTENT_CONTEXT_OPEN_WITH7 50196
#define IDC_CONTENT_CONTEXT_OPEN_WITH8 50197
#define IDC_CONTENT_CONTEXT_OPEN_WITH9 50198
#define IDC_CONTENT_CONTEXT_OPEN_WITH10 50199
#define IDC_CONTENT_CONTEXT_OPEN_WITH11 50200
#define IDC_CONTENT_CONTEXT_OPEN_WITH12 50201
#define IDC_CONTENT_CONTEXT_OPEN_WITH13 50202
#define IDC_CONTENT_CONTEXT_OPEN_WITH14 50203
#define IDC_CONTENT_CONTEXT_OPEN_WITH_LAST IDC_CONTENT_CONTEXT_OPEN_WITH14
// Context menu items that provide fast access to input methods.
#define IDC_CONTENT_CONTEXT_EMOJI 50210
// Context menu items in the bookmark bar
#define IDC_BOOKMARK_BAR_OPEN_ALL 51000
#define IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW 51001
#define IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO 51002
#define IDC_BOOKMARK_BAR_OPEN_INCOGNITO 51003
#define IDC_BOOKMARK_BAR_RENAME_FOLDER 51004
#define IDC_BOOKMARK_BAR_EDIT 51005
#define IDC_BOOKMARK_BAR_REMOVE 51006
#define IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK 51007
#define IDC_BOOKMARK_BAR_NEW_FOLDER 51008
#define IDC_BOOKMARK_MANAGER 51009
#define IDC_BOOKMARK_BAR_ALWAYS_SHOW 51010
#define IDC_BOOKMARK_BAR_SHOW_APPS_SHORTCUT 51011
#define IDC_BOOKMARK_BAR_UNDO 51012
#define IDC_BOOKMARK_BAR_REDO 51013
#define IDC_BOOKMARK_BAR_SHOW_MANAGED_BOOKMARKS 51014
// Context menu items for Sharing
#define IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE 51030
#define IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_MULTIPLE_DEVICES 51031
#define IDC_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_SINGLE_DEVICE 51032
#define IDC_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_MULTIPLE_DEVICES 51033
#define IDC_CONTENT_CONTEXT_GENERATE_QR_CODE 51034

// Context menu items in the status tray
#define IDC_STATUS_TRAY_KEEP_CHROME_RUNNING_IN_BACKGROUND 51100

// Context menu items for media router
#define IDC_MEDIA_ROUTER_ABOUT 51200
#define IDC_MEDIA_ROUTER_HELP 51201
#define IDC_MEDIA_ROUTER_LEARN_MORE 51202
#define IDC_MEDIA_ROUTER_REPORT_ISSUE 51203
#define IDC_MEDIA_ROUTER_ALWAYS_SHOW_TOOLBAR_ACTION 51204
#define IDC_MEDIA_ROUTER_CLOUD_SERVICES_TOGGLE 51205
#define IDC_MEDIA_ROUTER_SHOWN_BY_POLICY 51206
#define IDC_MEDIA_ROUTER_SHOW_IN_TOOLBAR 51207
#define IDC_MEDIA_ROUTER_TOGGLE_MEDIA_REMOTING 51208

// Context menu items for media stream status tray
#define IDC_MEDIA_STREAM_DEVICE_STATUS_TRAY 51300
#define IDC_MEDIA_CONTEXT_MEDIA_STREAM_CAPTURE_LIST_FIRST 51301
#define IDC_MEDIA_CONTEXT_MEDIA_STREAM_CAPTURE_LIST_LAST 51399
#define IDC_MEDIA_STREAM_DEVICE_ALWAYS_ALLOW 51400

// Protocol handler menu entries
#define IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_FIRST     52000
#define IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_LAST      52199
#define IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_SETTINGS  52200

// Open link in profile entries
#define IDC_OPEN_LINK_IN_PROFILE_FIRST 52300
#define IDC_OPEN_LINK_IN_PROFILE_LAST  52399

// Start smart text selection actions
#define IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION1 52400
#define IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION2 52401
#define IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION3 52402
#define IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION4 52403
#define IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION5 52404
#define IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION_LAST IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION5
// A gap here for new smart text selection actions.

// Accessibility labels
#define IDC_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_TOGGLE 52410
#define IDC_CONTENT_CONTEXT_ACCESSIBILITY_LABELS 52411
#define IDC_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_TOGGLE_ONCE 52412

// NOTE: The last valid command value is 57343 (0xDFFF)
// See http://msdn.microsoft.com/en-us/library/t2zechd4(VS.71).aspx

// Starting command id for menus showing bookmarks (such as the wrench menu).
// While command ids passed to Windows functions must not be higher than 0xDFFF,
// these IDs are not exposed to the native system and thus can be in this
// otherwise-reserved range. No command used in a menu (such as the wrench menu)
// should be higher than this, otherwise it'll conflict.
#define IDC_FIRST_BOOKMARK_MENU 0xE000

#endif  // CHROME_APP_CHROME_COMMAND_IDS_H_
