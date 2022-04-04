// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Subpages
import './appearance_page/appearance_fonts_page.js';
import './autofill_page/autofill_section.js';
import './autofill_page/password_check.js';
import './autofill_page/password_view.js';
import './autofill_page/passwords_device_section.js';
import './autofill_page/passwords_section.js';
import './autofill_page/payments_section.js';
import './clear_browsing_data_dialog/clear_browsing_data_dialog.js';
import './search_engines_page/search_engines_page.js';
import './privacy_page/privacy_guide/privacy_guide_description_item.js';
import './privacy_page/privacy_guide/privacy_guide_dialog.js';
import './privacy_page/privacy_guide/privacy_guide_history_sync_fragment.js';
import './privacy_page/privacy_guide/privacy_guide_msbb_fragment.js';
import './privacy_page/privacy_guide/privacy_guide_page.js';
import './privacy_page/security_keys_subpage.js';
import './privacy_page/security_keys_phones_subpage.js';
import './privacy_page/security_keys_phones_list.js';
import './privacy_page/security_keys_phones_dialog.js';
import './privacy_page/security_page.js';
import './site_settings/all_sites.js';
import './site_settings/site_data_details_subpage.js';
import './site_settings_page/site_settings_page.js';
import './site_settings/category_default_setting.js';
import './site_settings/category_setting_exceptions.js';
import './site_settings/chooser_exception_list.js';
import './site_settings/media_picker.js';
import './site_settings/pdf_documents.js';
import './site_settings/protocol_handlers.js';
import './site_settings/settings_category_default_radio_group.js';
import './site_settings/site_data.js';
import './site_settings/site_details.js';
import './site_settings/zoom_levels.js';
// <if expr="not chromeos_ash and not chromeos_lacros">
import './people_page/import_data_dialog.js';
// </if>
// <if expr="not chromeos_ash">
import './people_page/manage_profile.js';
// </if>
import './people_page/signout_dialog.js';
import './people_page/sync_controls.js';
import './people_page/sync_page.js';
// <if expr="use_nss_certs">
import 'chrome://resources/cr_components/certificate_manager/certificate_manager.js';
// </if>

// Sections
import './a11y_page/a11y_page.js';
import './downloads_page/downloads_page.js';
// <if expr="not chromeos_ash">
import './languages_page/languages_page.js';
// </if>
import './reset_page/reset_page.js';
// <if expr="not chromeos_ash">
import './system_page/system_page.js';
// </if>

// <if expr="not chromeos_ash and not is_macosx">
import './languages_page/edit_dictionary_page.js';

// </if>

export {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
export {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
export {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
export {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
export {CrSliderElement} from 'chrome://resources/cr_elements/cr_slider/cr_slider.js';
export {getToastManager} from 'chrome://resources/cr_elements/cr_toast/cr_toast_manager.js';
export {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
export {SettingsAppearanceFontsPageElement} from './appearance_page/appearance_fonts_page.js';
export {FontsBrowserProxy, FontsBrowserProxyImpl, FontsData} from './appearance_page/fonts_browser_proxy.js';
export {CountryDetailManager, CountryDetailManagerImpl, SettingsAddressEditDialogElement} from './autofill_page/address_edit_dialog.js';
export {SettingsAddressRemoveConfirmationDialogElement} from './autofill_page/address_remove_confirmation_dialog.js';
export {AutofillManagerImpl, AutofillManagerProxy, PersonalDataChangedListener} from './autofill_page/autofill_manager_proxy.js';
export {SettingsAutofillSectionElement} from './autofill_page/autofill_section.js';
// <if expr="chromeos_ash or chromeos_lacros">
export {BlockingRequestManager} from './autofill_page/blocking_request_manager.js';
// </if>
export {SettingsCreditCardEditDialogElement} from './autofill_page/credit_card_edit_dialog.js';
export {SettingsPasswordCheckElement} from './autofill_page/password_check.js';
export {SettingsPasswordCheckEditDialogElement} from './autofill_page/password_check_edit_dialog.js';
export {SettingsPasswordEditDisclaimerDialogElement} from './autofill_page/password_check_edit_disclaimer_dialog.js';
export {PasswordCheckListItemElement} from './autofill_page/password_check_list_item.js';
export {PasswordDialogMode, PasswordEditDialogElement} from './autofill_page/password_edit_dialog.js';
export {PasswordListItemElement} from './autofill_page/password_list_item.js';
export {PasswordMoveMultiplePasswordsToAccountDialogElement} from './autofill_page/password_move_multiple_passwords_to_account_dialog.js';
export {PasswordMoveToAccountDialogElement} from './autofill_page/password_move_to_account_dialog.js';
export {SettingsPasswordRemoveConfirmationDialogElement} from './autofill_page/password_remove_confirmation_dialog.js';
export {PasswordRemoveDialogElement} from './autofill_page/password_remove_dialog.js';
export {PasswordsDeviceSectionElement} from './autofill_page/passwords_device_section.js';
export {PasswordsExportDialogElement} from './autofill_page/passwords_export_dialog.js';
export {PasswordsSectionElement} from './autofill_page/passwords_section.js';
export {PaymentsManagerImpl, PaymentsManagerProxy} from './autofill_page/payments_manager_proxy.js';
export {SettingsPaymentsSectionElement} from './autofill_page/payments_section.js';
export {SettingsVirtualCardUnenrollDialogElement} from './autofill_page/virtual_card_unenroll_dialog.js';
// <if expr="_google_chrome and is_win">
export {ChromeCleanerScannerResults, ChromeCleanupFilePath, ChromeCleanupIdleReason, SettingsChromeCleanupPageElement} from './chrome_cleanup_page/chrome_cleanup_page.js';
export {ChromeCleanupProxy, ChromeCleanupProxyImpl} from './chrome_cleanup_page/chrome_cleanup_proxy.js';
export {CHROME_CLEANUP_DEFAULT_ITEMS_TO_SHOW, ItemsToRemoveListElement} from './chrome_cleanup_page/items_to_remove_list.js';
// </if>
export {ClearBrowsingDataBrowserProxy, ClearBrowsingDataBrowserProxyImpl, ClearBrowsingDataResult, InstalledApp} from './clear_browsing_data_dialog/clear_browsing_data_browser_proxy.js';
export {SettingsClearBrowsingDataDialogElement} from './clear_browsing_data_dialog/clear_browsing_data_dialog.js';
export {SettingsHistoryDeletionDialogElement} from './clear_browsing_data_dialog/history_deletion_dialog.js';
export {SettingsPasswordsDeletionDialogElement} from './clear_browsing_data_dialog/passwords_deletion_dialog.js';
export {ControlledButtonElement} from './controls/controlled_button.js';
export {SettingsCheckboxElement} from './controls/settings_checkbox.js';
export {SettingsRadioGroupElement} from './controls/settings_radio_group.js';
export {SettingsSliderElement} from './controls/settings_slider.js';
export {SettingsTextareaElement} from './controls/settings_textarea.js';
export {DownloadsBrowserProxy, DownloadsBrowserProxyImpl} from './downloads_page/downloads_browser_proxy.js';
export {SettingsDownloadsPageElement} from './downloads_page/downloads_page.js';
// <if expr="_google_chrome and is_win">
export {IncompatibleApplicationItemElement} from './incompatible_applications_page/incompatible_application_item.js';
export {ActionTypes, IncompatibleApplication, IncompatibleApplicationsBrowserProxy, IncompatibleApplicationsBrowserProxyImpl} from './incompatible_applications_page/incompatible_applications_browser_proxy.js';
export {SettingsIncompatibleApplicationsPageElement} from './incompatible_applications_page/incompatible_applications_page.js';
// </if>
// <if expr="not chromeos_ash">
export {SettingsAddLanguagesDialogElement} from './languages_page/add_languages_dialog.js';
// <if expr="not is_macosx">
export {SettingsEditDictionaryPageElement} from './languages_page/edit_dictionary_page.js';
// </if>
export {LanguagesBrowserProxy, LanguagesBrowserProxyImpl} from './languages_page/languages_browser_proxy.js';
export {SettingsLanguagesPageElement} from './languages_page/languages_page.js';
export {LanguageSettingsActionType, LanguageSettingsMetricsProxy, LanguageSettingsMetricsProxyImpl, LanguageSettingsPageImpressionType} from './languages_page/languages_settings_metrics_proxy.js';
export {kMenuCloseDelay, SettingsLanguagesSubpageElement} from './languages_page/languages_subpage.js';
export {LanguageHelper, LanguagesModel} from './languages_page/languages_types.js';
// </if>
// <if expr="not chromeos_ash and not chromeos_lacros">
export {BrowserProfile, ImportDataBrowserProxy, ImportDataBrowserProxyImpl, ImportDataStatus} from './people_page/import_data_browser_proxy.js';
export {SettingsImportDataDialogElement} from './people_page/import_data_dialog.js';
// </if>
// <if expr="not chromeos_ash">
export {SettingsManageProfileElement} from './people_page/manage_profile.js';
export {ManageProfileBrowserProxy, ManageProfileBrowserProxyImpl, ProfileShortcutStatus} from './people_page/manage_profile_browser_proxy.js';
// </if>
export {SettingsSyncControlsElement} from './people_page/sync_controls.js';
export {SettingsSyncEncryptionOptionsElement} from './people_page/sync_encryption_options.js';
export {SettingsSyncPageElement} from './people_page/sync_page.js';
export {SettingsCollapseRadioButtonElement} from './privacy_page/collapse_radio_button.js';
export {SettingsCookiesPageElement} from './privacy_page/cookies_page.js';
export {SettingsDoNotTrackToggleElement} from './privacy_page/do_not_track_toggle.js';
export {SettingsPersonalizationOptionsElement} from './privacy_page/personalization_options.js';
export {PrivacyGuideStep} from './privacy_page/privacy_guide/constants.js';
export {PrivacyGuideClearOnExitFragmentElement} from './privacy_page/privacy_guide/privacy_guide_clear_on_exit_fragment.js';
export {PrivacyGuideCompletionFragmentElement} from './privacy_page/privacy_guide/privacy_guide_completion_fragment.js';
export {PrivacyGuideDescriptionItemElement} from './privacy_page/privacy_guide/privacy_guide_description_item.js';
export {SettingsPrivacyGuideDialogElement} from './privacy_page/privacy_guide/privacy_guide_dialog.js';
export {PrivacyGuideHistorySyncFragmentElement} from './privacy_page/privacy_guide/privacy_guide_history_sync_fragment.js';
export {PrivacyGuideMsbbFragmentElement} from './privacy_page/privacy_guide/privacy_guide_msbb_fragment.js';
export {SettingsPrivacyGuidePageElement} from './privacy_page/privacy_guide/privacy_guide_page.js';
export {PrivacyGuideWelcomeFragmentElement} from './privacy_page/privacy_guide/privacy_guide_welcome_fragment.js';
export {SettingsSecureDnsElement} from './privacy_page/secure_dns.js';
export {SecureDnsInputElement} from './privacy_page/secure_dns_input.js';
export {BioEnrollDialogPage, SettingsSecurityKeysBioEnrollDialogElement} from './privacy_page/security_keys_bio_enroll_dialog.js';
export {Ctap2Status, SampleStatus, SecurityKeysBioEnrollProxy, SecurityKeysBioEnrollProxyImpl, SecurityKeysCredentialBrowserProxy, SecurityKeysCredentialBrowserProxyImpl, SecurityKeysPhone, SecurityKeysPhonesBrowserProxy, SecurityKeysPhonesBrowserProxyImpl, SecurityKeysPhonesList, SecurityKeysPINBrowserProxy, SecurityKeysPINBrowserProxyImpl, SecurityKeysResetBrowserProxy, SecurityKeysResetBrowserProxyImpl} from './privacy_page/security_keys_browser_proxy.js';
export {CredentialManagementDialogPage, SettingsSecurityKeysCredentialManagementDialogElement} from './privacy_page/security_keys_credential_management_dialog.js';
export {SecurityKeysPhonesSubpageElement} from './privacy_page/security_keys_phones_subpage.js';
export {ResetDialogPage, SettingsSecurityKeysResetDialogElement} from './privacy_page/security_keys_reset_dialog.js';
export {SetPINDialogPage, SettingsSecurityKeysSetPinDialogElement} from './privacy_page/security_keys_set_pin_dialog.js';
export {SafeBrowsingSetting, SettingsSecurityPageElement} from './privacy_page/security_page.js';
export {SettingsResetPageElement} from './reset_page/reset_page.js';
export {SettingsResetProfileDialogElement} from './reset_page/reset_profile_dialog.js';
export {SettingsOmniboxExtensionEntryElement} from './search_engines_page/omnibox_extension_entry.js';
export {SettingsSearchEngineDeleteConfirmationDialogElement} from './search_engines_page/search_engine_delete_confirmation_dialog.js';
export {SettingsSearchEngineEditDialogElement} from './search_engines_page/search_engine_edit_dialog.js';
export {SettingsSearchEngineEntryElement} from './search_engines_page/search_engine_entry.js';
export {SettingsSearchEnginesListElement} from './search_engines_page/search_engines_list.js';
export {SettingsSearchEnginesPageElement} from './search_engines_page/search_engines_page.js';
export {AddSiteDialogElement} from './site_settings/add_site_dialog.js';
export {AllSitesElement} from './site_settings/all_sites.js';
// <if expr="chromeos_ash">
export {AndroidInfoBrowserProxy, AndroidInfoBrowserProxyImpl, AndroidSmsInfo} from './site_settings/android_info_browser_proxy.js';
// </if>
export {CategoryDefaultSettingElement} from './site_settings/category_default_setting.js';
export {CategorySettingExceptionsElement} from './site_settings/category_setting_exceptions.js';
export {ChooserExceptionListElement} from './site_settings/chooser_exception_list.js';
export {ChooserExceptionListEntryElement} from './site_settings/chooser_exception_list_entry.js';
export {ChooserType, ContentSetting, ContentSettingsTypes, CookieControlsMode, NotificationSetting, SITE_EXCEPTION_WILDCARD, SiteSettingSource, SortMethod} from './site_settings/constants.js';
export {CookieDetails, cookieInfo} from './site_settings/cookie_info.js';
export {SettingsEditExceptionDialogElement} from './site_settings/edit_exception_dialog.js';
export {LocalDataBrowserProxy, LocalDataBrowserProxyImpl, LocalDataItem} from './site_settings/local_data_browser_proxy.js';
export {AppHandlerEntry, AppProtocolEntry, HandlerEntry, ProtocolEntry, ProtocolHandlersElement} from './site_settings/protocol_handlers.js';
export {SettingsCategoryDefaultRadioGroupElement} from './site_settings/settings_category_default_radio_group.js';
export {SiteDataElement} from './site_settings/site_data.js';
export {SiteDataDetailsSubpageElement} from './site_settings/site_data_details_subpage.js';
export {SiteDetailsElement} from './site_settings/site_details.js';
export {SiteDetailsPermissionElement} from './site_settings/site_details_permission.js';
export {SiteEntryElement} from './site_settings/site_entry.js';
export {SiteListElement} from './site_settings/site_list.js';
export {SiteListEntryElement} from './site_settings/site_list_entry.js';
export {ChooserException, ContentSettingProvider, CookiePrimarySetting, DefaultContentSetting, OriginInfo, RawChooserException, RawSiteException, RecentSitePermissions, SiteException, SiteGroup, SiteSettingsPrefsBrowserProxy, SiteSettingsPrefsBrowserProxyImpl, ZoomLevelEntry} from './site_settings/site_settings_prefs_browser_proxy.js';
export {WebsiteUsageBrowserProxy, WebsiteUsageBrowserProxyImpl} from './site_settings/website_usage_browser_proxy.js';
export {ZoomLevelsElement} from './site_settings/zoom_levels.js';
export {SettingsRecentSitePermissionsElement} from './site_settings_page/recent_site_permissions.js';
export {defaultSettingLabel} from './site_settings_page/site_settings_list.js';
export {SettingsSiteSettingsPageElement} from './site_settings_page/site_settings_page.js';
// <if expr="not chromeos_ash">
export {SettingsSystemPageElement} from './system_page/system_page.js';
export {SystemPageBrowserProxy, SystemPageBrowserProxyImpl} from './system_page/system_page_browser_proxy.js';

// </if>
