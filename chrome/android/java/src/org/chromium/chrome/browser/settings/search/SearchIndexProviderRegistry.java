// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.search;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.about_settings.AboutChromeSettings;
import org.chromium.chrome.browser.about_settings.LegalInformationSettings;
import org.chromium.chrome.browser.appearance.settings.AppearanceSettingsFragment;
import org.chromium.chrome.browser.autofill.options.AutofillOptionsFragment;
import org.chromium.chrome.browser.autofill.settings.AndroidPaymentAppsFragment;
import org.chromium.chrome.browser.autofill.settings.AutofillBuyNowPayLaterFragment;
import org.chromium.chrome.browser.autofill.settings.AutofillCardBenefitsFragment;
import org.chromium.chrome.browser.autofill.settings.AutofillIdentityDocsFragment;
import org.chromium.chrome.browser.autofill.settings.AutofillPaymentMethodsFragment;
import org.chromium.chrome.browser.autofill.settings.AutofillProfilesFragment;
import org.chromium.chrome.browser.autofill.settings.AutofillTravelFragment;
import org.chromium.chrome.browser.autofill.settings.FinancialAccountsManagementFragment;
import org.chromium.chrome.browser.autofill.settings.HomeOfTransactionsFragment;
import org.chromium.chrome.browser.autofill.settings.NonCardPaymentMethodsManagementFragment;
import org.chromium.chrome.browser.browsing_data.ClearBrowsingDataFragment;
import org.chromium.chrome.browser.commerce.PriceNotificationSettingsFragment;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchSettingsFragment;
import org.chromium.chrome.browser.download.settings.DownloadSettings;
import org.chromium.chrome.browser.glic.GlicSettings;
import org.chromium.chrome.browser.homepage.settings.HomepageSettings;
import org.chromium.chrome.browser.image_descriptions.ImageDescriptionsSettings;
import org.chromium.chrome.browser.language.settings.LanguageSettings;
import org.chromium.chrome.browser.night_mode.settings.ThemeSettingsFragment;
import org.chromium.chrome.browser.prefetch.settings.PreloadPagesSettingsFragment;
import org.chromium.chrome.browser.privacy.secure_dns.SecureDnsSettings;
import org.chromium.chrome.browser.privacy.settings.DoNotTrackSettings;
import org.chromium.chrome.browser.privacy.settings.PrivacySettings;
import org.chromium.chrome.browser.privacy_sandbox.AdMeasurementFragment;
import org.chromium.chrome.browser.privacy_sandbox.FledgeAllSitesFragment;
import org.chromium.chrome.browser.privacy_sandbox.FledgeBlockedSitesFragment;
import org.chromium.chrome.browser.privacy_sandbox.FledgeFragment;
import org.chromium.chrome.browser.privacy_sandbox.FledgeLearnMoreFragment;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxSettingsFragment;
import org.chromium.chrome.browser.privacy_sandbox.TopicsBlockedFragment;
import org.chromium.chrome.browser.privacy_sandbox.TopicsFragment;
import org.chromium.chrome.browser.privacy_sandbox.TopicsManageFragment;
import org.chromium.chrome.browser.safe_browsing.settings.SafeBrowsingSettingsFragment;
import org.chromium.chrome.browser.safe_browsing.settings.StandardProtectionSettingsFragment;
import org.chromium.chrome.browser.safety_check.SafetyCheckSettingsFragment;
import org.chromium.chrome.browser.safety_hub.SafetyHubFragment;
import org.chromium.chrome.browser.search_engines.settings.SearchEngineSettings;
import org.chromium.chrome.browser.search_engines.settings.SiteSearchSettings;
import org.chromium.chrome.browser.settings.MainSettings;
import org.chromium.chrome.browser.ssl.HttpsFirstModeSettingsFragment;
import org.chromium.chrome.browser.sync.settings.AccountManagementFragment;
import org.chromium.chrome.browser.sync.settings.GoogleServicesSettings;
import org.chromium.chrome.browser.sync.settings.ManageSyncSettings;
import org.chromium.chrome.browser.sync.settings.PersonalizeGoogleServicesSettings;
import org.chromium.chrome.browser.sync.settings.SignInPreference;
import org.chromium.chrome.browser.tasks.tab_management.TabArchiveSettingsFragment;
import org.chromium.chrome.browser.tasks.tab_management.TabsSettings;
import org.chromium.chrome.browser.toolbar.adaptive.settings.AdaptiveToolbarSettingsFragment;
import org.chromium.chrome.browser.tracing.settings.DeveloperSettings;
import org.chromium.chrome.browser.tracing.settings.TracingCategoriesSettings;
import org.chromium.chrome.browser.tracing.settings.TracingSettings;
import org.chromium.components.browser_ui.accessibility.AccessibilitySettings;
import org.chromium.components.browser_ui.settings.search.SearchIndexProvider;
import org.chromium.components.browser_ui.site_settings.ChosenObjectSettings;
import org.chromium.components.browser_ui.site_settings.CookieSettings;
import org.chromium.components.browser_ui.site_settings.GroupedWebsitesSettings;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettings;
import org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings;
import org.chromium.components.browser_ui.site_settings.SiteSettings;
import org.chromium.components.page_info.PageInfoAdPersonalizationSettings;
import org.chromium.components.page_info.PageInfoCookiesSettings;

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
                    MainSettings.SEARCH_INDEX_DATA_PROVIDER,
                    AboutChromeSettings.SEARCH_INDEX_DATA_PROVIDER,
                    AdaptiveToolbarSettingsFragment.SEARCH_INDEX_DATA_PROVIDER,
                    AutofillOptionsFragment.SEARCH_INDEX_DATA_PROVIDER,
                    ContextualSearchSettingsFragment.SEARCH_INDEX_DATA_PROVIDER,
                    GlicSettings.SEARCH_INDEX_DATA_PROVIDER,
                    DoNotTrackSettings.SEARCH_INDEX_DATA_PROVIDER,
                    HomepageSettings.SEARCH_INDEX_DATA_PROVIDER,
                    LegalInformationSettings.SEARCH_INDEX_DATA_PROVIDER,
                    SearchEngineSettings.SEARCH_INDEX_DATA_PROVIDER,
                    SiteSearchSettings.SEARCH_INDEX_DATA_PROVIDER,
                    PersonalizeGoogleServicesSettings.SEARCH_INDEX_DATA_PROVIDER,
                    SecureDnsSettings.SEARCH_INDEX_DATA_PROVIDER,
                    TabArchiveSettingsFragment.SEARCH_INDEX_DATA_PROVIDER,
                    AdMeasurementFragment.SEARCH_INDEX_DATA_PROVIDER,
                    FledgeAllSitesFragment.SEARCH_INDEX_DATA_PROVIDER,
                    FledgeFragment.SEARCH_INDEX_DATA_PROVIDER,
                    FledgeLearnMoreFragment.SEARCH_INDEX_DATA_PROVIDER,
                    FledgeBlockedSitesFragment.SEARCH_INDEX_DATA_PROVIDER,
                    TopicsFragment.SEARCH_INDEX_DATA_PROVIDER,
                    TopicsBlockedFragment.SEARCH_INDEX_DATA_PROVIDER,
                    TopicsManageFragment.SEARCH_INDEX_DATA_PROVIDER,
                    PrivacySandboxSettingsFragment.SEARCH_INDEX_DATA_PROVIDER,
                    ChosenObjectSettings.SEARCH_INDEX_DATA_PROVIDER,
                    GroupedWebsitesSettings.SEARCH_INDEX_DATA_PROVIDER,
                    PageInfoAdPersonalizationSettings.SEARCH_INDEX_DATA_PROVIDER,
                    PageInfoCookiesSettings.SEARCH_INDEX_DATA_PROVIDER,
                    CookieSettings.SEARCH_INDEX_DATA_PROVIDER,
                    SingleCategorySettings.SEARCH_INDEX_DATA_PROVIDER,
                    SingleWebsiteSettings.SEARCH_INDEX_DATA_PROVIDER,
                    SiteSettings.SEARCH_INDEX_DATA_PROVIDER,
                    AccessibilitySettings.SEARCH_INDEX_DATA_PROVIDER,
                    ImageDescriptionsSettings.SEARCH_INDEX_DATA_PROVIDER,
                    PreloadPagesSettingsFragment.SEARCH_INDEX_DATA_PROVIDER,
                    SafeBrowsingSettingsFragment.SEARCH_INDEX_DATA_PROVIDER,
                    StandardProtectionSettingsFragment.SEARCH_INDEX_DATA_PROVIDER,
                    SafetyHubFragment.SEARCH_INDEX_DATA_PROVIDER,
                    AccountManagementFragment.SEARCH_INDEX_DATA_PROVIDER,
                    AndroidPaymentAppsFragment.SEARCH_INDEX_DATA_PROVIDER,
                    AppearanceSettingsFragment.SEARCH_INDEX_DATA_PROVIDER,
                    AutofillBuyNowPayLaterFragment.SEARCH_INDEX_DATA_PROVIDER,
                    AutofillCardBenefitsFragment.SEARCH_INDEX_DATA_PROVIDER,
                    AutofillIdentityDocsFragment.SEARCH_INDEX_DATA_PROVIDER,
                    AutofillPaymentMethodsFragment.SEARCH_INDEX_DATA_PROVIDER,
                    AutofillProfilesFragment.SEARCH_INDEX_DATA_PROVIDER,
                    AutofillTravelFragment.SEARCH_INDEX_DATA_PROVIDER,
                    HomeOfTransactionsFragment.SEARCH_INDEX_DATA_PROVIDER,
                    ClearBrowsingDataFragment.SEARCH_INDEX_DATA_PROVIDER,
                    FinancialAccountsManagementFragment.SEARCH_INDEX_DATA_PROVIDER,
                    GoogleServicesSettings.SEARCH_INDEX_DATA_PROVIDER,
                    HttpsFirstModeSettingsFragment.SEARCH_INDEX_DATA_PROVIDER,
                    LanguageSettings.SEARCH_INDEX_DATA_PROVIDER,
                    NonCardPaymentMethodsManagementFragment.SEARCH_INDEX_DATA_PROVIDER,
                    PriceNotificationSettingsFragment.SEARCH_INDEX_DATA_PROVIDER,
                    PrivacySettings.SEARCH_INDEX_DATA_PROVIDER,
                    SafetyCheckSettingsFragment.SEARCH_INDEX_DATA_PROVIDER,
                    TabsSettings.SEARCH_INDEX_DATA_PROVIDER,
                    ManageSyncSettings.SEARCH_INDEX_DATA_PROVIDER,
                    SignInPreference.SEARCH_INDEX_DATA_PROVIDER,
                    ThemeSettingsFragment.SEARCH_INDEX_DATA_PROVIDER,
                    DownloadSettings.SEARCH_INDEX_DATA_PROVIDER,
                    DeveloperSettings.SEARCH_INDEX_DATA_PROVIDER,
                    TracingSettings.SEARCH_INDEX_DATA_PROVIDER,
                    TracingCategoriesSettings.SEARCH_INDEX_DATA_PROVIDER);
}
