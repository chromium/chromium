// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.search;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.settings.search.SearchIndexProvider;

import java.util.List;

/**
 * A central registry that holds the list of all SearchIndexProvider instances. This is the single
 * source of truth for the search indexing process.
 */
@NullMarked
public final class SearchIndexProviderRegistry {
    /**
     * The list of all providers that can be indexed for settings search.
     *
     * <p>When you create a new searchable PreferenceFragment, you must add its
     * SEARCH_INDEX_DATA_PROVIDER to this list.
     */
    public static final List<SearchIndexProvider> ALL_PROVIDERS =
            List.of(
                    // MainSettings always comes at the top. This fixes the order of the header
                    // groups in which they will be shown in the search results since the index
                    // uses LinkedHashMap internally.
                    org.chromium.chrome.browser.settings.MainSettings.SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.about_settings.AboutChromeSettings
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.toolbar.adaptive.settings
                            .AdaptiveToolbarSettingsFragment.SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.autofill.options.AutofillOptionsFragment
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.contextualsearch.ContextualSearchSettingsFragment
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.glic.GlicSettings.SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.privacy.settings.DoNotTrackSettings
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.homepage.settings.HomepageSettings
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.about_settings.LegalInformationSettings
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.sync.settings.PersonalizeGoogleServicesSettings
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.privacy.secure_dns.SecureDnsSettings
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.tasks.tab_management.TabArchiveSettingsFragment
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.privacy_sandbox.AdMeasurementFragment
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.privacy_sandbox.FledgeAllSitesFragment
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.privacy_sandbox.FledgeFragment
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.privacy_sandbox.FledgeLearnMoreFragment
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.privacy_sandbox.FledgeBlockedSitesFragment
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.privacy_sandbox.TopicsFragment
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.privacy_sandbox.TopicsBlockedFragment
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.privacy_sandbox.TopicsManageFragment
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxSettingsFragment
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.components.browser_ui.site_settings.ChosenObjectSettings
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.components.browser_ui.site_settings.GroupedWebsitesSettings
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.components.page_info.PageInfoAdPersonalizationSettings
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.components.page_info.PageInfoCookiesSettings
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.components.browser_ui.site_settings.CookieSettings
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.components.browser_ui.site_settings.SingleCategorySettings
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.components.browser_ui.site_settings.SiteSettings
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.components.browser_ui.accessibility.AccessibilitySettings
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.image_descriptions.ImageDescriptionsSettings
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.prefetch.settings.PreloadPagesSettingsFragment
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.safe_browsing.settings.SafeBrowsingSettingsFragment
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.safe_browsing.settings
                            .StandardProtectionSettingsFragment.SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.safety_hub.SafetyHubFragment
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.sync.settings.AccountManagementFragment
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.autofill.settings.AndroidPaymentAppsFragment
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.appearance.settings.AppearanceSettingsFragment
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.autofill.settings.AutofillBuyNowPayLaterFragment
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.autofill.settings.AutofillCardBenefitsFragment
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.autofill.settings.AutofillPaymentMethodsFragment
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.autofill.settings.AutofillProfilesFragment
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.browsing_data.ClearBrowsingDataFragment
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.autofill.settings
                            .FinancialAccountsManagementFragment.SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.sync.settings.GoogleServicesSettings
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.ssl.HttpsFirstModeSettingsFragment
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.language.settings.LanguageSettings
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.autofill.settings
                            .NonCardPaymentMethodsManagementFragment.SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.commerce.PriceNotificationSettingsFragment
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.privacy.settings.PrivacySettings
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.safety_check.SafetyCheckSettingsFragment
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.tasks.tab_management.TabsSettings
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.sync.settings.ManageSyncSettings
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.sync.settings.SignInPreference
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.night_mode.settings.ThemeSettingsFragment
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.download.settings.DownloadSettings
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.tracing.settings.DeveloperSettings
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.tracing.settings.TracingSettings
                            .SEARCH_INDEX_DATA_PROVIDER,
                    org.chromium.chrome.browser.tracing.settings.TracingCategoriesSettings
                            .SEARCH_INDEX_DATA_PROVIDER);
}
