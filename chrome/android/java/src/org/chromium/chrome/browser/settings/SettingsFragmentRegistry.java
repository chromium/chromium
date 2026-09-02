// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.net.Uri;
import android.os.Bundle;
import android.util.ArrayMap;

import androidx.annotation.VisibleForTesting;
import androidx.fragment.app.Fragment;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.about_settings.AboutChromeSettings;
import org.chromium.chrome.browser.about_settings.LegalInformationSettings;
import org.chromium.chrome.browser.appearance.settings.AppearanceSettingsFragment;
import org.chromium.chrome.browser.autofill.settings.AndroidPaymentAppsFragment;
import org.chromium.chrome.browser.autofill.settings.AutofillAndPasswordsFragment;
import org.chromium.chrome.browser.autofill.settings.AutofillBuyNowPayLaterFragment;
import org.chromium.chrome.browser.autofill.settings.AutofillCardBenefitsFragment;
import org.chromium.chrome.browser.autofill.settings.AutofillIdentityDocsFragment;
import org.chromium.chrome.browser.autofill.settings.AutofillPaymentMethodsFragment;
import org.chromium.chrome.browser.autofill.settings.AutofillProfilesFragment;
import org.chromium.chrome.browser.autofill.settings.AutofillShoppingFragment;
import org.chromium.chrome.browser.autofill.settings.AutofillTravelFragment;
import org.chromium.chrome.browser.autofill.settings.FinancialAccountsManagementFragment;
import org.chromium.chrome.browser.autofill.settings.NonCardPaymentMethodsManagementFragment;
import org.chromium.chrome.browser.autofill.settings.options.AutofillOptionsFragment;
import org.chromium.chrome.browser.autofill.settings.personal_context.AutofillPersonalContextFragment;
import org.chromium.chrome.browser.browsing_data.ClearBrowsingDataFragment;
import org.chromium.chrome.browser.download.settings.DownloadSettings;
import org.chromium.chrome.browser.glic.GlicSettings;
import org.chromium.chrome.browser.homepage.settings.HomepageSettings;
import org.chromium.chrome.browser.image_descriptions.ImageDescriptionsSettings;
import org.chromium.chrome.browser.language.settings.AlwaysTranslateListFragment;
import org.chromium.chrome.browser.language.settings.LanguageSettings;
import org.chromium.chrome.browser.language.settings.NeverTranslateListFragment;
import org.chromium.chrome.browser.language.settings.SelectLanguageFragment;
import org.chromium.chrome.browser.night_mode.NightModeMetrics;
import org.chromium.chrome.browser.night_mode.settings.ThemeSettingsFragment;
import org.chromium.chrome.browser.prefetch.settings.ExtendedPreloadingSettingsFragment;
import org.chromium.chrome.browser.prefetch.settings.PreloadPagesSettingsFragment;
import org.chromium.chrome.browser.prefetch.settings.StandardPreloadingSettingsFragment;
import org.chromium.chrome.browser.privacy.secure_dns.SecureDnsSettings;
import org.chromium.chrome.browser.privacy.settings.DoNotTrackSettings;
import org.chromium.chrome.browser.privacy.settings.PrivacySettings;
import org.chromium.chrome.browser.privacy_guide.PrivacyGuideFragment;
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
import org.chromium.chrome.browser.tasks.tab_management.TabArchiveSettingsFragment;
import org.chromium.chrome.browser.tasks.tab_management.TabsSettings;
import org.chromium.chrome.browser.toolbar.adaptive.settings.AdaptiveToolbarSettingsFragment;
import org.chromium.chrome.browser.tracing.settings.DeveloperSettings;
import org.chromium.chrome.browser.tracing.settings.TracingSettings;
import org.chromium.components.browser_ui.accessibility.AccessibilitySettings;
import org.chromium.components.browser_ui.site_settings.AllSiteSettings;
import org.chromium.components.browser_ui.site_settings.ChosenObjectSettings;
import org.chromium.components.browser_ui.site_settings.CookieSettings;
import org.chromium.components.browser_ui.site_settings.GroupedWebsitesSettings;
import org.chromium.components.browser_ui.site_settings.LocationPermissionSubpageSettings;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettings;
import org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings;
import org.chromium.components.browser_ui.site_settings.SiteSettings;
import org.chromium.components.browser_ui.site_settings.StorageAccessSubpageSettings;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Locale;
import java.util.Set;
import java.util.function.Consumer;

/** Centralized registry mapping chrome://settings URLs to Fragment classes. */
@NullMarked
public class SettingsFragmentRegistry {
    // Path maps are initialized statically to ensure constant-time URL routing.
    @VisibleForTesting
    static final ArrayMap<String, Class<? extends Fragment>> sPathToFragmentMap = new ArrayMap<>();

    @VisibleForTesting
    static final ArrayMap<Class<? extends Fragment>, String> sFragmentToPathMap = new ArrayMap<>();

    @VisibleForTesting
    static final ArrayMap<String, String> sQueryParamToArgKeyMap = new ArrayMap<>();

    @VisibleForTesting
    static final ArrayMap<String, String> sArgKeyToQueryParamMap = new ArrayMap<>();

    @VisibleForTesting
    static final ArrayMap<Class<? extends Fragment>, Consumer<Bundle>> sDefaultArgsProviders =
            new ArrayMap<>();

    static {
        // Root path mappings pointing to the top-level main settings fragment.
        registerMapping("", MainSettings.class);
        registerMapping("/", MainSettings.class);

        // Search Results Fragment
        registerMapping("/results", SearchResultsPreferenceFragment.class);

        // Multi Column Settings Categories
        // --------------------------------

        // You and Google
        //
        // TODO(crbug.com/542745585): Handle /account differently based on local state
        // since it's not a static page which is always available.
        registerMapping("/account", ManageSyncSettings.class);
        registerMapping("/googleServices", GoogleServicesSettings.class);

        // Basics
        registerMapping("/search", SearchEngineSettings.class);
        registerMapping("/search/siteSearch", SiteSearchSettings.class);

        // Privacy & Security
        registerMapping("/privacy", PrivacySettings.class);
        registerMapping("/privacyGuide", PrivacyGuideFragment.class);
        registerMapping("/clearBrowsingData", ClearBrowsingDataFragment.class);
        registerMapping("/cookies", CookieSettings.class);
        registerMapping("/doNotTrack", DoNotTrackSettings.class);
        registerMapping("/preloadPages", PreloadPagesSettingsFragment.class);
        registerMapping("/preloadPages/standard", StandardPreloadingSettingsFragment.class);
        registerMapping("/preloadPages/extended", ExtendedPreloadingSettingsFragment.class);

        // Security
        registerMapping("/safeBrowsing", SafeBrowsingSettingsFragment.class);
        registerMapping("/safeBrowsing/enhanced", EnhancedProtectionSettingsFragment.class);
        registerMapping("/safeBrowsing/standard", StandardProtectionSettingsFragment.class);
        registerMapping("/httpsFirstMode", HttpsFirstModeSettingsFragment.class);
        registerMapping("/secureDns", SecureDnsSettings.class);

        // Safety Check / Safety Hub
        registerMapping("/safetyCheck", SafetyHubFragment.class);
        registerMapping("/notifications", SafetyCheckSettingsFragment.class);

        // Autofill & Passwords
        registerMapping("/autofill", AutofillAndPasswordsFragment.class);
        registerMapping("/payments", AutofillPaymentMethodsFragment.class);
        registerMapping("/payments/nonCardMethods", NonCardPaymentMethodsManagementFragment.class);
        registerMapping("/payments/financialAccounts", FinancialAccountsManagementFragment.class);
        registerMapping("/cardBenefits", AutofillCardBenefitsFragment.class);
        registerMapping("/cardBenefits/bnpl", AutofillBuyNowPayLaterFragment.class);
        registerMapping("/paymentApps", AndroidPaymentAppsFragment.class);
        registerMapping("/addresses", AutofillProfilesFragment.class);
        registerMapping("/autofill/identityDocs", AutofillIdentityDocsFragment.class);
        registerMapping("/autofill/travel", AutofillTravelFragment.class);
        registerMapping("/autofill/shopping", AutofillShoppingFragment.class);
        registerMapping("/autofill/personalContext", AutofillPersonalContextFragment.class);
        registerMapping("/autofill/settings", AutofillOptionsFragment.class);

        // Tabs and tab groups
        registerMapping("/tabs", TabsSettings.class);
        registerMapping("/inactiveTabs", TabArchiveSettingsFragment.class);

        // Homepage
        registerMapping("/homepage", HomepageSettings.class);

        // Appearance
        registerMapping("/appearance", AppearanceSettingsFragment.class);
        registerMapping("/theme", ThemeSettingsFragment.class);
        registerMapping("/toolbar", AdaptiveToolbarSettingsFragment.class);

        // Accessibility
        registerMapping("/accessibility", AccessibilitySettings.class);
        registerMapping("/savedZoomForSites", ImageDescriptionsSettings.class);

        // Content / Site Settings
        registerMapping("/siteSettings", SiteSettings.class);
        registerMapping("/siteSettings/category", SingleCategorySettings.class);
        registerMapping("/allSites", AllSiteSettings.class);
        registerMapping("/allSites/group", GroupedWebsitesSettings.class);
        registerMapping("/siteDetails", SingleWebsiteSettings.class);
        registerMapping("/storageAccess", StorageAccessSubpageSettings.class);
        registerMapping("/locationPermission", LocationPermissionSubpageSettings.class);
        registerMapping("/chosenObject", ChosenObjectSettings.class);

        // Languages, Downloads, Tabs, Homepage
        registerMapping("/languages", LanguageSettings.class);
        registerMapping("/languages/select", SelectLanguageFragment.class);
        registerMapping("/languages/alwaysTranslate", AlwaysTranslateListFragment.class);
        registerMapping("/languages/neverTranslate", NeverTranslateListFragment.class);
        registerMapping("/downloads", DownloadSettings.class);

        // About & Developer
        registerMapping("/about", AboutChromeSettings.class);
        registerMapping("/about/legal", LegalInformationSettings.class);
        registerMapping("/developer", DeveloperSettings.class);
        registerMapping("/developer/tracing", TracingSettings.class);

        // Glic
        registerMapping("/ai/gemini", GlicSettings.class);

        // Parameter translations mapping URL query string keys to Fragment
        // argument extra keys.
        registerParameterMapping("site", SingleWebsiteSettings.EXTRA_SITE_ADDRESS);
        registerParameterMapping("category", SingleCategorySettings.EXTRA_CATEGORY);
        registerParameterMapping("title", SingleCategorySettings.EXTRA_TITLE);
        registerParameterMapping("group", GroupedWebsitesSettings.EXTRA_GROUP);
        registerParameterMapping(
                "potentialLanguages", SelectLanguageFragment.KEY_POTENTIAL_LANGUAGES);
        registerParameterMapping("referrer", AutofillAndPasswordsFragment.EXTRA_REFERRER);

        // Register default argument providers cleanly without hardcoding in URL parsing logic
        sDefaultArgsProviders.put(
                ThemeSettingsFragment.class,
                bundle -> {
                    if (!bundle.containsKey(ThemeSettingsFragment.KEY_THEME_SETTINGS_ENTRY)) {
                        bundle.putInt(
                                ThemeSettingsFragment.KEY_THEME_SETTINGS_ENTRY,
                                NightModeMetrics.ThemeSettingsEntry.SETTINGS);
                    }
                });
    }

    private static void registerMapping(
            String path, Class<? extends Fragment> detailFragmentClass) {
        // Normalize lookup keys to lowercase US locale so path matching
        // remains case-insensitive and insensitive to system locale settings
        // (avoiding Turkish dotted/dotless i issues).
        String normalizedPath = path.toLowerCase(Locale.US);
        sPathToFragmentMap.put(normalizedPath, detailFragmentClass);

        // Store canonical subpage path preserving original casing for clean
        // reverse URL generation.
        if (!normalizedPath.equals("/")) {
            String canonicalPath = path.startsWith("/") ? path.substring(1) : path;
            sFragmentToPathMap.put(detailFragmentClass, canonicalPath);
        }
    }

    private static void registerParameterMapping(String queryParam, String argKey) {
        sQueryParamToArgKeyMap.put(queryParam, argKey);
        sArgKeyToQueryParamMap.put(argKey, queryParam);
    }

    public static void registerMappingForTesting(
            String path, Class<? extends Fragment> detailFragmentClass) {
        registerMapping(path, detailFragmentClass);
    }

    /**
     * Resolves a chrome://settings URL string to a target Fragment class.
     *
     * @param url Target URL to resolve.
     * @return Resolved Fragment class token, or null if invalid.
     */
    public static @Nullable Class<? extends Fragment> getFragmentClassForUrl(String url) {
        String scheme;
        String host;
        String path;
        try {
            Uri uri = Uri.parse(url);
            scheme = uri.getScheme();
            host = uri.getHost();
            path = uri.getPath();
        } catch (UnsupportedOperationException
                | IllegalArgumentException
                | IndexOutOfBoundsException e) {
            // Malformed URIs safely fall back to null.
            return null;
        }

        // Strict scheme validation prevents cross-origin or local file
        // scheme injection.
        if (!UrlUtilities.isChromeScheme(scheme)) {
            return null;
        }

        // Strict host validation ensures only settings pages are routed
        // through this registry.
        if (!UrlConstants.SETTINGS_HOST.equalsIgnoreCase(host)) {
            return null;
        }

        if (path == null) path = "";

        // Normalize path using Locale.US for case-insensitive matching
        // regardless of device locale.
        path = path.toLowerCase(Locale.US);

        // Strip trailing slash for consistency (e.g.
        // "chrome://settings/appearance/" ->
        // "chrome://settings/appearance").
        if (path.endsWith("/")) {
            path = path.substring(0, path.length() - 1);
        }

        return sPathToFragmentMap.get(path);
    }

    /**
     * Extracts URL query parameters into a Fragment argument Bundle.
     *
     * @param url URL string containing optional query parameters.
     * @return Populated {@link Bundle} containing typed parameter extras.
     */
    public static Bundle parseUrlArguments(String url) {
        Bundle bundle = new Bundle();
        if (url.isEmpty()) return bundle;

        try {
            Uri uri = Uri.parse(url);
            if (!uri.isHierarchical()) return bundle;

            Set<String> queryNames = uri.getQueryParameterNames();
            for (String param : queryNames) {
                String val = uri.getQueryParameter(param);
                if (val == null) continue;

                // Map query parameter key to argument bundle key if registered,
                // otherwise keep original.
                String argKey = sQueryParamToArgKeyMap.getOrDefault(param, param);
                bundle.putString(argKey, val);
            }
        } catch (UnsupportedOperationException
                | IllegalArgumentException
                | IndexOutOfBoundsException e) {
            // Malformed URIs safely fail with an empty bundle to prevent partial state.
            return new Bundle();
        }

        // Populate default mandatory argument extras for specific target
        // fragments if omitted in the URL query parameters. Some fragments
        // (such as ThemeSettingsFragment) enforce assertions on mandatory
        // entry point extras during fragment creation.
        applyDefaultArguments(url, bundle);
        return bundle;
    }

    /** Applies registered default arguments for the resolved target fragment. */
    private static void applyDefaultArguments(String url, Bundle bundle) {
        Class<? extends Fragment> fragmentClass = getFragmentClassForUrl(url);
        if (fragmentClass != null && sDefaultArgsProviders.containsKey(fragmentClass)) {
            sDefaultArgsProviders.get(fragmentClass).accept(bundle);
        }
    }

    /**
     * Creates a canonical URL for a Fragment class and argument Bundle.
     *
     * @param fragmentClass Target fragment class to represent.
     * @param args Optional argument bundle containing query parameters.
     * @return Canonical chrome://settings URL string, or null if unmapped.
     */
    public static @Nullable String createUrlForFragment(
            Class<? extends Fragment> fragmentClass, @Nullable Bundle args) {
        String path = getUrlPathForFragmentClass(fragmentClass);
        if (path == null && !MainSettings.class.equals(fragmentClass)) {
            return null;
        }

        // TODO(mwoj): This will resolve to chrome://settings, but we should consistently pick
        // chrome-native://settings or chrome://settings so the Urls don't mix and match when
        // clicking through preference fragments or manually entering Urls.
        String chromeSettingsUrl = UrlConstants.CHROME_URL_PREFIX + UrlConstants.SETTINGS_HOST;
        String targetUrl = chromeSettingsUrl + "/" + (path != null ? path : "");

        // If there are no supplied arguments, just return the page.
        // (e.g., chrome://settings/about).
        if (args == null || args.isEmpty()) {
            return targetUrl;
        }

        // Otherwise, bake the arguments into url.
        // (e.g., chrome://settings/siteDetails?site=foo.com)
        Uri.Builder builder = Uri.parse(targetUrl).buildUpon();

        // Sort the keys here for unit tests, so that generated strings
        // will be consistent.
        List<String> sortedKeys = new ArrayList<>(args.keySet());
        Collections.sort(sortedKeys);
        for (String key : sortedKeys) {
            Object val = args.get(key);
            if (val == null) continue;

            if (!(val instanceof String || val instanceof Number || val instanceof Boolean)) {
                continue;
            }

            // Map argument extra key back to canonical URL query param key.
            String queryParam = sArgKeyToQueryParamMap.getOrDefault(key, key);
            builder.appendQueryParameter(queryParam, String.valueOf(val));
        }
        return builder.build().toString();
    }

    /**
     * Returns the canonical URL path for a given Fragment class.
     *
     * @param fragmentClass Fragment class token to look up.
     * @return Canonical path string without leading slash, or null if unmapped.
     */
    public static @Nullable String getUrlPathForFragmentClass(
            Class<? extends Fragment> fragmentClass) {
        return sFragmentToPathMap.get(fragmentClass);
    }
}
