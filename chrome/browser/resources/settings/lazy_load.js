// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Subpages
import './appearance_page/appearance_fonts_page.js';
import './autofill_page/password_check.js';
import './autofill_page/passwords_section.js';
import './autofill_page/passwords_device_section.js';
import './autofill_page/payments_section.js';
import './clear_browsing_data_dialog/clear_browsing_data_dialog.js';
import './search_engines_page/search_engines_page.js';
import './privacy_page/cookies_page.js';
import './privacy_page/security_keys_subpage.js';
import './privacy_page/security_page.js';
import './site_settings/all_sites.js';
import './site_settings/site_data_details_subpage.js';
import './site_settings_page/site_settings_page.js';
import './site_settings/category_default_setting.js';
import './site_settings/category_setting_exceptions.js';
import './site_settings/chooser_exception_list.js';
import './site_settings/media_picker.js';
import './site_settings/pdf_documents.js';
import './site_settings/settings_category_default_radio_group.js';
import './site_settings/site_data.js';
import './site_settings/site_details.js';
import './site_settings/zoom_levels.js';
// <if expr="not chromeos">
import './people_page/import_data_dialog.js';
import './people_page/manage_profile.js';
// </if>
import './people_page/signout_dialog.m.js';
import './people_page/sync_controls.m.js';
import './people_page/sync_page.m.js';
// <if expr="use_nss_certs">
import 'chrome://resources/cr_components/certificate_manager/certificate_manager.js';
// </if>

// Sections
import './a11y_page/a11y_page.js';
import './downloads_page/downloads_page.js';
import './languages_page/languages_page.js';
import './printing_page/printing_page.js';
import './reset_page/reset_page.js';
// <if expr="not chromeos">
import './system_page/system_page.js';
// </if>

// <if expr="not is_macosx">
import './languages_page/edit_dictionary_page.js';

// </if>

export {getToastManager} from 'chrome://resources/cr_elements/cr_toast/cr_toast_manager.m.js';
export {FontsBrowserProxy, FontsBrowserProxyImpl} from './appearance_page/fonts_browser_proxy.m.js';
export {CountryDetailManagerImpl} from './autofill_page/address_edit_dialog.js';
export {AutofillManager, AutofillManagerImpl} from './autofill_page/autofill_section.js';
// <if expr="chromeos">
export {BlockingRequestManager} from './autofill_page/blocking_request_manager.js';
// </if>
export {PaymentsManager, PaymentsManagerImpl} from './autofill_page/payments_section.js';
// <if expr="_google_chrome and is_win">
export {ChromeCleanupIdleReason} from './chrome_cleanup_page/chrome_cleanup_page.js';
export {ChromeCleanupProxy, ChromeCleanupProxyImpl} from './chrome_cleanup_page/chrome_cleanup_proxy.js';
export {CHROME_CLEANUP_DEFAULT_ITEMS_TO_SHOW} from './chrome_cleanup_page/items_to_remove_list.js';
// </if>
export {ClearBrowsingDataBrowserProxy, ClearBrowsingDataBrowserProxyImpl, InstalledApp} from './clear_browsing_data_dialog/clear_browsing_data_browser_proxy.js';
export {DownloadsBrowserProxyImpl} from './downloads_page/downloads_browser_proxy.js';
// <if expr="_google_chrome and is_win">
export {IncompatibleApplication, IncompatibleApplicationsBrowserProxyImpl} from './incompatible_applications_page/incompatible_applications_browser_proxy.js';
// </if>
export {LanguagesBrowserProxy, LanguagesBrowserProxyImpl} from './languages_page/languages_browser_proxy.m.js';
// <if expr="chromeos">
export {LanguagesMetricsProxy, LanguagesMetricsProxyImpl, LanguagesPageInteraction} from './languages_page/languages_metrics_proxy.js';
// </if>
export {kMenuCloseDelay} from './languages_page/languages_page.js';
// <if expr="not chromeos">
export {ImportDataBrowserProxyImpl, ImportDataStatus} from './people_page/import_data_browser_proxy.js';
export {ManageProfileBrowserProxyImpl, ProfileShortcutStatus} from './people_page/manage_profile_browser_proxy.js';
// </if>
export {BioEnrollDialogPage} from './privacy_page/security_keys_bio_enroll_dialog.js';
export {Ctap2Status, SampleStatus, SecurityKeysBioEnrollProxyImpl, SecurityKeysCredentialBrowserProxyImpl, SecurityKeysPINBrowserProxyImpl, SecurityKeysResetBrowserProxyImpl} from './privacy_page/security_keys_browser_proxy.js';
export {CredentialManagementDialogPage} from './privacy_page/security_keys_credential_management_dialog.js';
export {ResetDialogPage} from './privacy_page/security_keys_reset_dialog.js';
export {SetPINDialogPage} from './privacy_page/security_keys_set_pin_dialog.js';
export {SafeBrowsingSetting} from './privacy_page/security_page.js';
// <if expr="chromeos">
export {AndroidInfoBrowserProxyImpl} from './site_settings/android_info_browser_proxy.js';
// </if>
export {ChooserType, ContentSetting, ContentSettingsTypes, CookieControlsMode, SITE_EXCEPTION_WILDCARD, SiteSettingSource, SortMethod} from './site_settings/constants.js';
export {cookieInfo} from './site_settings/cookie_info.js';
export {CookieList, LocalDataBrowserProxy, LocalDataBrowserProxyImpl, LocalDataItem} from './site_settings/local_data_browser_proxy.js';
export {HandlerEntry, ProtocolEntry} from './site_settings/protocol_handlers.js';
export {kControlledByLookup} from './site_settings/site_settings_behavior.js';
export {ContentSettingProvider, DefaultContentSetting, RawChooserException, RawSiteException, RecentSitePermissions, SiteException, SiteGroup, SiteSettingsPrefsBrowserProxy, SiteSettingsPrefsBrowserProxyImpl, ZoomLevelEntry} from './site_settings/site_settings_prefs_browser_proxy.js';
export {WebsiteUsageBrowserProxyImpl} from './site_settings/website_usage_browser_proxy.js';
export {defaultSettingLabel} from './site_settings_page/site_settings_list.js';
// <if expr="not chromeos">
export {SystemPageBrowserProxyImpl} from './system_page/system_page_browser_proxy.js';

// </if>
