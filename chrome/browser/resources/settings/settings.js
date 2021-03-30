// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './settings_ui/settings_ui.js';

export {PluralStringProxyImpl as SettingsPluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
export {AboutPageBrowserProxy, AboutPageBrowserProxyImpl, PromoteUpdaterStatus, UpdateStatus} from './about_page/about_page_browser_proxy.js';
export {AppearanceBrowserProxy, AppearanceBrowserProxyImpl} from './appearance_page/appearance_browser_proxy.js';
export {MultiStoreExceptionEntry} from './autofill_page/multi_store_exception_entry.js';
export {MultiStorePasswordUiEntry} from './autofill_page/multi_store_password_ui_entry.js';
export {PasswordManagerImpl, PasswordManagerProxy} from './autofill_page/password_manager_proxy.js';
// <if expr="not chromeos and not lacros">
export {DefaultBrowserBrowserProxyImpl} from './default_browser_page/default_browser_browser_proxy.js';
// </if>
export {ExtensionControlBrowserProxyImpl} from './extension_control_browser_proxy.js';
export {HatsBrowserProxy, HatsBrowserProxyImpl} from './hats_browser_proxy.js';
export {loadTimeData} from './i18n_setup.js';
export {LifetimeBrowserProxy, LifetimeBrowserProxyImpl} from './lifetime_browser_proxy.js';
export {MetricsBrowserProxy, MetricsBrowserProxyImpl, PrivacyElementInteractions, SafeBrowsingInteractions, SafetyCheckInteractions} from './metrics_browser_proxy.js';
export {OnStartupBrowserProxy, OnStartupBrowserProxyImpl} from './on_startup_page/on_startup_browser_proxy.js';
export {EDIT_STARTUP_URL_EVENT} from './on_startup_page/startup_url_entry.js';
export {StartupUrlsPageBrowserProxy, StartupUrlsPageBrowserProxyImpl} from './on_startup_page/startup_urls_page_browser_proxy.js';
export {OpenWindowProxy, OpenWindowProxyImpl} from './open_window_proxy.js';
export {pageVisibility, setPageVisibilityForTesting} from './page_visibility.js';
// <if expr="chromeos">
export {AccountManagerBrowserProxyImpl} from './people_page/account_manager_browser_proxy.js';
// </if>
export {ProfileInfoBrowserProxyImpl} from './people_page/profile_info_browser_proxy.js';
export {MAX_SIGNIN_PROMO_IMPRESSION} from './people_page/sync_account_control.js';
export {PageStatus, StatusAction, StoredAccount, SyncBrowserProxy, SyncBrowserProxyImpl, SyncStatus} from './people_page/sync_browser_proxy.js';
export {prefToString, stringToPrefValue} from './prefs/pref_util.js';
export {PrefsBehavior} from './prefs/prefs_behavior.js';
export {CrSettingsPrefs} from './prefs/prefs_types.js';
export {MetricsReporting, PrivacyPageBrowserProxy, PrivacyPageBrowserProxyImpl, ResolverOption, SecureDnsMode, SecureDnsSetting, SecureDnsUiManagementMode} from './privacy_page/privacy_page_browser_proxy.js';
export {ResetBrowserProxyImpl} from './reset_page/reset_browser_proxy.js';
export {buildRouter, routes} from './route.js';
export {Route, Router} from './router.js';
export {SafetyCheckBrowserProxy, SafetyCheckBrowserProxyImpl, SafetyCheckCallbackConstants, SafetyCheckChromeCleanerStatus, SafetyCheckExtensionsStatus, SafetyCheckParentStatus, SafetyCheckPasswordsStatus, SafetyCheckSafeBrowsingStatus, SafetyCheckUpdatesStatus} from './safety_check_page/safety_check_browser_proxy.js';
export {SafetyCheckIconStatus} from './safety_check_page/safety_check_child.js';
export {SearchEngine, SearchEnginesBrowserProxy, SearchEnginesBrowserProxyImpl, SearchEnginesInfo} from './search_engines_page/search_engines_browser_proxy.js';
export {getSearchManager, SearchRequest, setSearchManagerForTesting} from './search_settings.js';
