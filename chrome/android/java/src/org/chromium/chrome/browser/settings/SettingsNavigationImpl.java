// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

import androidx.fragment.app.Fragment;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.IntentUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.about_settings.AboutChromeSettings;
import org.chromium.chrome.browser.about_settings.LegalInformationSettings;
import org.chromium.chrome.browser.appearance.settings.AppearanceSettingsFragment;
import org.chromium.chrome.browser.autofill.settings.AndroidPaymentAppsFragment;
import org.chromium.chrome.browser.autofill.settings.AutofillBuyNowPayLaterFragment;
import org.chromium.chrome.browser.autofill.settings.AutofillCardBenefitsFragment;
import org.chromium.chrome.browser.autofill.settings.AutofillIdentityDocsFragment;
import org.chromium.chrome.browser.autofill.settings.AutofillPaymentMethodsFragment;
import org.chromium.chrome.browser.autofill.settings.AutofillProfilesFragment;
import org.chromium.chrome.browser.autofill.settings.AutofillShoppingFragment;
import org.chromium.chrome.browser.autofill.settings.AutofillTravelFragment;
import org.chromium.chrome.browser.autofill.settings.FinancialAccountsManagementFragment;
import org.chromium.chrome.browser.autofill.settings.HomeOfTransactionsFragment;
import org.chromium.chrome.browser.autofill.settings.NonCardPaymentMethodsManagementFragment;
import org.chromium.chrome.browser.autofill.settings.options.AutofillOptionsFragment;
import org.chromium.chrome.browser.autofill.settings.personal_context.AutofillPersonalContextFragment;
import org.chromium.chrome.browser.browsing_data.ClearBrowsingDataFragment;
import org.chromium.chrome.browser.commerce.PriceNotificationSettingsFragment;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchSettingsFragment;
import org.chromium.chrome.browser.download.settings.DownloadSettings;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.glic.GlicActorLoginPermissionsFragment;
import org.chromium.chrome.browser.glic.GlicSettings;
import org.chromium.chrome.browser.homepage.settings.HomepageSettings;
import org.chromium.chrome.browser.image_descriptions.ImageDescriptionsSettings;
import org.chromium.chrome.browser.language.settings.LanguageSettings;
import org.chromium.chrome.browser.night_mode.settings.ThemeSettingsFragment;
import org.chromium.chrome.browser.prefetch.settings.ExtendedPreloadingSettingsFragment;
import org.chromium.chrome.browser.prefetch.settings.PreloadPagesSettingsFragment;
import org.chromium.chrome.browser.prefetch.settings.StandardPreloadingSettingsFragment;
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
import org.chromium.chrome.browser.safe_browsing.settings.EnhancedProtectionSettingsFragment;
import org.chromium.chrome.browser.safe_browsing.settings.SafeBrowsingSettingsFragment;
import org.chromium.chrome.browser.safe_browsing.settings.StandardProtectionSettingsFragment;
import org.chromium.chrome.browser.safety_check.SafetyCheckSettingsFragment;
import org.chromium.chrome.browser.safety_hub.SafetyHubFragment;
import org.chromium.chrome.browser.search_engines.settings.SearchEngineSettings;
import org.chromium.chrome.browser.search_engines.settings.SiteSearchSettings;
import org.chromium.chrome.browser.settings.search.SearchResultsPreferenceFragment;
import org.chromium.chrome.browser.ssl.HttpsFirstModeSettingsFragment;
import org.chromium.chrome.browser.sync.settings.GoogleServicesSettings;
import org.chromium.chrome.browser.sync.settings.ManageSyncSettings;
import org.chromium.chrome.browser.sync.settings.PersonalizeGoogleServicesSettings;
import org.chromium.chrome.browser.tasks.tab_management.TabArchiveSettingsFragment;
import org.chromium.chrome.browser.tasks.tab_management.TabsSettings;
import org.chromium.chrome.browser.toolbar.adaptive.settings.AdaptiveToolbarSettingsFragment;
import org.chromium.chrome.browser.toolbar.settings.AddressBarSettingsFragment;
import org.chromium.chrome.browser.tracing.settings.DeveloperSettings;
import org.chromium.chrome.browser.tracing.settings.TracingCategoriesSettings;
import org.chromium.chrome.browser.tracing.settings.TracingSettings;
import org.chromium.components.browser_ui.accessibility.AccessibilitySettings;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.site_settings.AllSiteSettings;
import org.chromium.components.browser_ui.site_settings.ChosenObjectSettings;
import org.chromium.components.browser_ui.site_settings.CookieSettings;
import org.chromium.components.browser_ui.site_settings.GroupedWebsitesSettings;
import org.chromium.components.browser_ui.site_settings.LocationPermissionSubpageSettings;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettings;
import org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings;
import org.chromium.components.browser_ui.site_settings.SiteSettings;
import org.chromium.components.browser_ui.site_settings.StorageAccessSubpageSettings;
import org.chromium.components.page_info.PageInfoAdPersonalizationSettings;
import org.chromium.components.page_info.PageInfoCookiesSettings;

/** Implementation class for launching a {@link SettingsActivity}. */
@NullMarked
public class SettingsNavigationImpl implements SettingsNavigation {

    /** Instantiated through SettingsNavigationFactory. */
    SettingsNavigationImpl() {}

    @Override
    public void startSettings(Context context) {
        startSettings(context, SettingsFragment.MAIN);
    }

    @Override
    public void startSettings(Context context, @SettingsFragment int settingsFragment) {
        startSettings(context, settingsFragment, /* addToBackStack= */ false);
    }

    @Override
    public void startSettings(
            Context context, @SettingsFragment int settingsFragment, boolean addToBackStack) {
        Bundle fragmentArgs = null;
        switch (settingsFragment) {
            case SettingsFragment.CLEAR_BROWSING_DATA:
                fragmentArgs =
                        ClearBrowsingDataFragment.createFragmentArgs(context.getClass().getName());
                break;
            case SettingsFragment.ABOUT_CHROME:
            case SettingsFragment.ACCESSIBILITY:
            case SettingsFragment.ADAPTIVE_TOOLBAR:
            case SettingsFragment.ADDRESS_BAR:
            case SettingsFragment.AD_MEASUREMENT:
            case SettingsFragment.ALL_SITES:
            case SettingsFragment.ANDROID_PAYMENT_APPS:
            case SettingsFragment.APPEARANCE:
            case SettingsFragment.AUTOFILL_BUY_NOW_PAY_LATER:
            case SettingsFragment.AUTOFILL_CARD_BENEFITS:
            case SettingsFragment.AUTOFILL_IDENTITY_DOCS:
            case SettingsFragment.AUTOFILL_OPTIONS:
            case SettingsFragment.AUTOFILL_PERSONAL_CONTEXT:
            case SettingsFragment.AUTOFILL_PROFILES:
            case SettingsFragment.AUTOFILL_SHOPPING:
            case SettingsFragment.AUTOFILL_TRAVEL:
            case SettingsFragment.CHOSEN_OBJECT:
            case SettingsFragment.CONTEXTUAL_SEARCH:
            case SettingsFragment.COOKIES:
            case SettingsFragment.DEVELOPER:
            case SettingsFragment.DO_NOT_TRACK:
            case SettingsFragment.DOWNLOADS:
            case SettingsFragment.FINANCIAL_ACCOUNTS:
            case SettingsFragment.GLIC:
            case SettingsFragment.GLIC_PERMISSIONS:
            case SettingsFragment.GOOGLE_SERVICES:
            case SettingsFragment.GROUPED_WEBSITES:
            case SettingsFragment.HOME_OF_TRANSACTIONS:
            case SettingsFragment.HOMEPAGE:
            case SettingsFragment.HTTPS_FIRST_MODE:
            case SettingsFragment.IMAGE_DESCRIPTIONS:
            case SettingsFragment.LANGUAGE:
            case SettingsFragment.LEGAL_INFORMATION:
            case SettingsFragment.LOCATION_PERMISSION:
            case SettingsFragment.MAIN:
            case SettingsFragment.MANAGE_SYNC:
            case SettingsFragment.NON_CARD_PAYMENT_METHODS:
            case SettingsFragment.PAGE_INFO_AD_PERSONALIZATION:
            case SettingsFragment.PAGE_INFO_COOKIES:
            case SettingsFragment.PAYMENT_METHODS:
            case SettingsFragment.PERSONALIZE_GOOGLE_SERVICES:
            case SettingsFragment.PRELOAD_PAGES:
            case SettingsFragment.PRELOAD_PAGES_EXTENDED:
            case SettingsFragment.PRELOAD_PAGES_STANDARD:
            case SettingsFragment.PRICE_NOTIFICATION:
            case SettingsFragment.PRIVACY:
            case SettingsFragment.PRIVACY_SANDBOX_FLEDGE:
            case SettingsFragment.PRIVACY_SANDBOX_FLEDGE_ALL_SITES:
            case SettingsFragment.PRIVACY_SANDBOX_FLEDGE_BLOCKED_SITES:
            case SettingsFragment.PRIVACY_SANDBOX_FLEDGE_LEARN_MORE:
            case SettingsFragment.PRIVACY_SANDBOX_SETTINGS:
            case SettingsFragment.PRIVACY_SANDBOX_TOPICS:
            case SettingsFragment.PRIVACY_SANDBOX_TOPICS_BLOCKED:
            case SettingsFragment.PRIVACY_SANDBOX_TOPICS_MANAGE:
            case SettingsFragment.SAFE_BROWSING:
            case SettingsFragment.SAFE_BROWSING_ENHANCED:
            case SettingsFragment.SAFE_BROWSING_STANDARD:
            case SettingsFragment.SAFETY_CHECK:
            case SettingsFragment.SAFETY_CHECK_SETTINGS:
            case SettingsFragment.SEARCH_ENGINE:
            case SettingsFragment.SEARCH_RESULTS:
            case SettingsFragment.SECURE_DNS:
            case SettingsFragment.SINGLE_CATEGORY:
            case SettingsFragment.SINGLE_WEBSITE:
            case SettingsFragment.SITE:
            case SettingsFragment.SITE_SEARCH:
            case SettingsFragment.STORAGE_ACCESS:
            case SettingsFragment.TAB_ARCHIVE:
            case SettingsFragment.TABS:
            case SettingsFragment.THEME:
            case SettingsFragment.TRACING:
            case SettingsFragment.TRACING_CATEGORIES:
                break;
        }
        startSettings(
                context, getFragmentClassFromEnum(settingsFragment), fragmentArgs, addToBackStack);
    }

    @Override
    public void startSettings(Context context, @Nullable Class<? extends Fragment> fragment) {
        startSettings(context, fragment, null);
    }

    @Override
    public void startSettings(
            Context context,
            @Nullable Class<? extends Fragment> fragment,
            @Nullable Bundle fragmentArgs) {
        startSettings(context, fragment, fragmentArgs, /* addToBackStack= */ false);
    }

    @Override
    public void startSettings(
            Context context,
            @Nullable Class<? extends Fragment> fragment,
            @Nullable Bundle fragmentArgs,
            boolean addToBackStack) {
        startSettings(context, fragment, fragmentArgs, addToBackStack, /* tag= */ null);
    }

    @Override
    public void startSettings(
            Context context,
            @Nullable Class<? extends Fragment> fragment,
            @Nullable Bundle fragmentArgs,
            boolean addToBackStack,
            @Nullable String tag) {
        if (ChromeFeatureList.sSettingsInTab.isEnabled()) {
            Activity activity = ActivityUtil.getActivityFromContext(context);
            // Some components pass a non-Activity context (e.g. AccessibilitySettings).
            if (activity == null) {
                activity = ApplicationStatus.getLastTrackedFocusedActivity();
            }
            assert activity != null;
            SettingsHostFragment settingsHostFragment = SettingsHostFragment.get(activity);
            assert settingsHostFragment != null;
            // A null `fragment` implies the main settings page, so pass null to showFragment().
            Fragment targetFragment =
                    fragment != null
                            ? Fragment.instantiate(context, fragment.getName(), fragmentArgs)
                            : null;
            if (settingsHostFragment.showFragment(targetFragment, addToBackStack, tag)) {
                // TODO(crbug.com/521895796): Once we have a mechanism to open the settings
                // tab when it is closed, switch to that, and move this "return" outside this block.
                // Until then we fall back to SettingsActivity, just in case.
                return;
            }
        }
        Intent intent = createSettingsIntent(context, fragment, fragmentArgs, addToBackStack, tag);
        IntentUtils.safeStartActivity(context, intent);
    }

    @Override
    public Intent createSettingsIntent(
            Context context, @Nullable Class<? extends Fragment> fragment) {
        return createSettingsIntent(context, fragment, null);
    }

    @Override
    public Intent createSettingsIntent(
            Context context,
            @Nullable Class<? extends Fragment> fragment,
            @Nullable Bundle fragmentArgs) {
        return createSettingsIntent(context, fragment, fragmentArgs, /* addToBackStack= */ false);
    }

    @Override
    public Intent createSettingsIntent(
            Context context,
            @Nullable Class<? extends Fragment> fragment,
            @Nullable Bundle fragmentArgs,
            boolean addToBackStack) {
        return createSettingsIntent(
                context, fragment, fragmentArgs, addToBackStack, /* tag= */ null);
    }

    @Override
    public Intent createSettingsIntent(
            Context context,
            @Nullable Class<? extends Fragment> fragment,
            @Nullable Bundle fragmentArgs,
            boolean addToBackStack,
            @Nullable String tag) {
        String fragmentName = fragment == null ? null : fragment.getName();
        return SettingsIntentUtil.createIntent(
                context, fragmentName, fragmentArgs, addToBackStack, tag);
    }

    @Override
    public Intent createSettingsIntent(
            Context context, @SettingsFragment int fragment, @Nullable Bundle fragmentArgs) {
        return createSettingsIntent(context, getFragmentClassFromEnum(fragment), fragmentArgs);
    }

    private static @Nullable Class<? extends Fragment> getFragmentClassFromEnum(
            @SettingsFragment int fragment) {
        switch (fragment) {
            case SettingsFragment.ABOUT_CHROME:
                return AboutChromeSettings.class;
            case SettingsFragment.ACCESSIBILITY:
                return AccessibilitySettings.class;
            case SettingsFragment.ADAPTIVE_TOOLBAR:
                return AdaptiveToolbarSettingsFragment.class;
            case SettingsFragment.ADDRESS_BAR:
                return AddressBarSettingsFragment.class;
            case SettingsFragment.AD_MEASUREMENT:
                return AdMeasurementFragment.class;
            case SettingsFragment.ALL_SITES:
                return AllSiteSettings.class;
            case SettingsFragment.ANDROID_PAYMENT_APPS:
                return AndroidPaymentAppsFragment.class;
            case SettingsFragment.APPEARANCE:
                return AppearanceSettingsFragment.class;
            case SettingsFragment.AUTOFILL_BUY_NOW_PAY_LATER:
                return AutofillBuyNowPayLaterFragment.class;
            case SettingsFragment.AUTOFILL_CARD_BENEFITS:
                return AutofillCardBenefitsFragment.class;
            case SettingsFragment.AUTOFILL_IDENTITY_DOCS:
                return AutofillIdentityDocsFragment.class;
            case SettingsFragment.AUTOFILL_OPTIONS:
                return AutofillOptionsFragment.class;
            case SettingsFragment.AUTOFILL_PERSONAL_CONTEXT:
                return AutofillPersonalContextFragment.class;
            case SettingsFragment.AUTOFILL_PROFILES:
                return AutofillProfilesFragment.class;
            case SettingsFragment.AUTOFILL_SHOPPING:
                return AutofillShoppingFragment.class;
            case SettingsFragment.AUTOFILL_TRAVEL:
                return AutofillTravelFragment.class;
            case SettingsFragment.CHOSEN_OBJECT:
                return ChosenObjectSettings.class;
            case SettingsFragment.CLEAR_BROWSING_DATA:
                return ClearBrowsingDataFragment.class;
            case SettingsFragment.CONTEXTUAL_SEARCH:
                return ContextualSearchSettingsFragment.class;
            case SettingsFragment.COOKIES:
                return CookieSettings.class;
            case SettingsFragment.DEVELOPER:
                return DeveloperSettings.class;
            case SettingsFragment.DO_NOT_TRACK:
                return DoNotTrackSettings.class;
            case SettingsFragment.DOWNLOADS:
                return DownloadSettings.class;
            case SettingsFragment.FINANCIAL_ACCOUNTS:
                return FinancialAccountsManagementFragment.class;
            case SettingsFragment.GLIC:
                return GlicSettings.class;
            case SettingsFragment.GLIC_PERMISSIONS:
                return GlicActorLoginPermissionsFragment.class;
            case SettingsFragment.GOOGLE_SERVICES:
                return GoogleServicesSettings.class;
            case SettingsFragment.GROUPED_WEBSITES:
                return GroupedWebsitesSettings.class;
            case SettingsFragment.HOME_OF_TRANSACTIONS:
                return HomeOfTransactionsFragment.class;
            case SettingsFragment.HOMEPAGE:
                return HomepageSettings.class;
            case SettingsFragment.HTTPS_FIRST_MODE:
                return HttpsFirstModeSettingsFragment.class;
            case SettingsFragment.IMAGE_DESCRIPTIONS:
                return ImageDescriptionsSettings.class;
            case SettingsFragment.LANGUAGE:
                return LanguageSettings.class;
            case SettingsFragment.LEGAL_INFORMATION:
                return LegalInformationSettings.class;
            case SettingsFragment.LOCATION_PERMISSION:
                return LocationPermissionSubpageSettings.class;
            case SettingsFragment.MAIN:
                return null; // The main settings page.
            case SettingsFragment.MANAGE_SYNC:
                return ManageSyncSettings.class;
            case SettingsFragment.NON_CARD_PAYMENT_METHODS:
                return NonCardPaymentMethodsManagementFragment.class;
            case SettingsFragment.PAGE_INFO_AD_PERSONALIZATION:
                return PageInfoAdPersonalizationSettings.class;
            case SettingsFragment.PAGE_INFO_COOKIES:
                return PageInfoCookiesSettings.class;
            case SettingsFragment.PAYMENT_METHODS:
                return AutofillPaymentMethodsFragment.class;
            case SettingsFragment.PERSONALIZE_GOOGLE_SERVICES:
                return PersonalizeGoogleServicesSettings.class;
            case SettingsFragment.PRELOAD_PAGES:
                return PreloadPagesSettingsFragment.class;
            case SettingsFragment.PRELOAD_PAGES_EXTENDED:
                return ExtendedPreloadingSettingsFragment.class;
            case SettingsFragment.PRELOAD_PAGES_STANDARD:
                return StandardPreloadingSettingsFragment.class;
            case SettingsFragment.PRICE_NOTIFICATION:
                return PriceNotificationSettingsFragment.class;
            case SettingsFragment.PRIVACY:
                return PrivacySettings.class;
            case SettingsFragment.PRIVACY_SANDBOX_FLEDGE:
                return FledgeFragment.class;
            case SettingsFragment.PRIVACY_SANDBOX_FLEDGE_ALL_SITES:
                return FledgeAllSitesFragment.class;
            case SettingsFragment.PRIVACY_SANDBOX_FLEDGE_BLOCKED_SITES:
                return FledgeBlockedSitesFragment.class;
            case SettingsFragment.PRIVACY_SANDBOX_FLEDGE_LEARN_MORE:
                return FledgeLearnMoreFragment.class;
            case SettingsFragment.PRIVACY_SANDBOX_SETTINGS:
                return PrivacySandboxSettingsFragment.class;
            case SettingsFragment.PRIVACY_SANDBOX_TOPICS:
                return TopicsFragment.class;
            case SettingsFragment.PRIVACY_SANDBOX_TOPICS_BLOCKED:
                return TopicsBlockedFragment.class;
            case SettingsFragment.PRIVACY_SANDBOX_TOPICS_MANAGE:
                return TopicsManageFragment.class;
            case SettingsFragment.SAFE_BROWSING:
                return SafeBrowsingSettingsFragment.class;
            case SettingsFragment.SAFE_BROWSING_ENHANCED:
                return EnhancedProtectionSettingsFragment.class;
            case SettingsFragment.SAFE_BROWSING_STANDARD:
                return StandardProtectionSettingsFragment.class;
            case SettingsFragment.SAFETY_CHECK:
                return SafetyHubFragment.class;
            case SettingsFragment.SAFETY_CHECK_SETTINGS:
                return SafetyCheckSettingsFragment.class;
            case SettingsFragment.SEARCH_ENGINE:
                return SearchEngineSettings.class;
            case SettingsFragment.SEARCH_RESULTS:
                return SearchResultsPreferenceFragment.class;
            case SettingsFragment.SECURE_DNS:
                return SecureDnsSettings.class;
            case SettingsFragment.SINGLE_CATEGORY:
                return SingleCategorySettings.class;
            case SettingsFragment.SINGLE_WEBSITE:
                return SingleWebsiteSettings.class;
            case SettingsFragment.SITE:
                return SiteSettings.class;
            case SettingsFragment.SITE_SEARCH:
                return SiteSearchSettings.class;
            case SettingsFragment.STORAGE_ACCESS:
                return StorageAccessSubpageSettings.class;
            case SettingsFragment.TAB_ARCHIVE:
                return TabArchiveSettingsFragment.class;
            case SettingsFragment.TABS:
                return TabsSettings.class;
            case SettingsFragment.THEME:
                return ThemeSettingsFragment.class;
            case SettingsFragment.TRACING:
                return TracingSettings.class;
            case SettingsFragment.TRACING_CATEGORIES:
                return TracingCategoriesSettings.class;
        }
        assert false;
        return null;
    }

    @Override
    public void finishCurrentSettings(Fragment fragment) {
        Activity activity = fragment.getActivity();
        if (activity != null) {
            ((SettingsActivity) activity).finishCurrentSettings(fragment);
        }
    }

    @Override
    public void executePendingNavigations(Activity activity) {
        ((SettingsActivity) activity).executePendingNavigations();
    }
}
