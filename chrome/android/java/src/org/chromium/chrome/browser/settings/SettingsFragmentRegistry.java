// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.util.ArrayMap;

import androidx.annotation.VisibleForTesting;
import androidx.fragment.app.Fragment;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.about_settings.AboutChromeSettings;
import org.chromium.chrome.browser.about_settings.LegalInformationSettings;
import org.chromium.chrome.browser.appearance.settings.AppearanceSettingsFragment;
import org.chromium.chrome.browser.autofill.settings.AndroidPaymentAppsFragment;
import org.chromium.chrome.browser.autofill.settings.AutofillAndPasswordsFragment;
import org.chromium.chrome.browser.autofill.settings.AutofillCardBenefitsFragment;
import org.chromium.chrome.browser.autofill.settings.AutofillPaymentMethodsFragment;
import org.chromium.chrome.browser.autofill.settings.AutofillProfilesFragment;
import org.chromium.chrome.browser.autofill.settings.options.AutofillOptionsFragment;
import org.chromium.chrome.browser.browsing_data.ClearBrowsingDataFragment;
import org.chromium.chrome.browser.download.settings.DownloadSettings;
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
import org.chromium.components.browser_ui.site_settings.CookieSettings;
import org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings;
import org.chromium.components.browser_ui.site_settings.SiteSettings;

import java.util.Locale;

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

        // Privacy
        registerMapping("/privacy", PrivacySettings.class);
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
        // TODO(mwoj): There are some missing here, but it's not obvious which.
        registerMapping("/autofill", AutofillAndPasswordsFragment.class);
        registerMapping("/payments", AutofillPaymentMethodsFragment.class);
        registerMapping("/cardBenefits", AutofillCardBenefitsFragment.class);
        registerMapping("/paymentApps", AndroidPaymentAppsFragment.class);
        registerMapping("/addresses", AutofillProfilesFragment.class);
        registerMapping("/autofillSettings", AutofillOptionsFragment.class);

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
        registerMapping("/allSites", AllSiteSettings.class);

        // Languages, Downloads, Tabs, Homepage
        registerMapping("/languages", LanguageSettings.class);
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
}
