// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.os.Bundle;

import androidx.fragment.app.Fragment;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.about_settings.AboutChromeSettings;
import org.chromium.chrome.browser.about_settings.LegalInformationSettings;
import org.chromium.chrome.browser.appearance.settings.AppearanceSettingsFragment;
import org.chromium.chrome.browser.browsing_data.ClearBrowsingDataFragment;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.night_mode.NightModeMetrics;
import org.chromium.chrome.browser.night_mode.settings.ThemeSettingsFragment;
import org.chromium.chrome.browser.prefetch.settings.ExtendedPreloadingSettingsFragment;
import org.chromium.chrome.browser.prefetch.settings.PreloadPagesSettingsFragment;
import org.chromium.chrome.browser.prefetch.settings.StandardPreloadingSettingsFragment;
import org.chromium.chrome.browser.privacy.settings.PrivacySettings;
import org.chromium.chrome.browser.safe_browsing.settings.EnhancedProtectionSettingsFragment;
import org.chromium.chrome.browser.safe_browsing.settings.SafeBrowsingSettingsFragment;
import org.chromium.chrome.browser.safe_browsing.settings.StandardProtectionSettingsFragment;
import org.chromium.chrome.browser.tracing.settings.DeveloperSettings;
import org.chromium.chrome.browser.tracing.settings.TracingSettings;
import org.chromium.components.browser_ui.site_settings.GroupedWebsitesSettings;
import org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings;
import org.chromium.components.browser_ui.site_settings.Website;
import org.chromium.components.browser_ui.site_settings.WebsiteAddress;
import org.chromium.components.browser_ui.site_settings.WebsiteGroup;

import java.util.Collections;
import java.util.Map;

/** Unit tests for {@link SettingsFragmentRegistry}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(qualifiers = "sw600dp")
@EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
public class SettingsFragmentRegistryTest {

    @Test
    public void testRegistryInitialization() throws Exception {
        Map<String, Class<? extends Fragment>> pathMap =
                SettingsFragmentRegistry.sPathToFragmentMap;

        // Verify root path
        assertEquals(MainSettings.class, pathMap.get(""));
        assertEquals(MainSettings.class, pathMap.get("/"));

        // Verify some notable mappings
        assertEquals(PrivacySettings.class, pathMap.get("/privacy"));
        assertEquals(AppearanceSettingsFragment.class, pathMap.get("/appearance"));
        assertEquals(ThemeSettingsFragment.class, pathMap.get("/theme"));
        assertEquals(SafeBrowsingSettingsFragment.class, pathMap.get("/safebrowsing"));

        // Verify canonical path mapping
        Map<Class<? extends Fragment>, String> fragmentMap =
                SettingsFragmentRegistry.sFragmentToPathMap;
        assertEquals("privacy", fragmentMap.get(PrivacySettings.class));
        assertEquals("appearance", fragmentMap.get(AppearanceSettingsFragment.class));
        assertEquals("theme", fragmentMap.get(ThemeSettingsFragment.class));
    }

    @Test
    public void testRegisterMappingForTesting() throws Exception {
        Map<String, Class<? extends Fragment>> pathMap =
                SettingsFragmentRegistry.sPathToFragmentMap;
        Map<Class<? extends Fragment>, String> fragmentMap =
                SettingsFragmentRegistry.sFragmentToPathMap;

        String testPath = "/test/custom";
        Class<? extends Fragment> testClass = Fragment.class;

        // Ensure it doesn't exist yet
        assertFalse(pathMap.containsKey("/test/custom"));

        // Register the new mapping
        SettingsFragmentRegistry.registerMappingForTesting(testPath, testClass);

        // Verify it was added
        assertEquals(testClass, pathMap.get("/test/custom"));
        assertEquals("test/custom", fragmentMap.get(testClass));

        // Clean up
        pathMap.remove("/test/custom");
        fragmentMap.remove(testClass);
    }

    @Test
    public void testParameterMappings() throws Exception {
        Map<String, String> queryMap = SettingsFragmentRegistry.sQueryParamToArgKeyMap;
        Map<String, String> argMap = SettingsFragmentRegistry.sArgKeyToQueryParamMap;

        assertEquals(SingleWebsiteSettings.EXTRA_SITE_ADDRESS, queryMap.get("site"));
        assertEquals("site", argMap.get(SingleWebsiteSettings.EXTRA_SITE_ADDRESS));
    }

    @Test
    public void testRegisterMappingCaseInsensitive() throws Exception {
        // Registering with mixed case should be lowercased in the registry map
        SettingsFragmentRegistry.registerMappingForTesting("/Test/Case", Fragment.class);

        Map<String, Class<? extends Fragment>> pathMap =
                SettingsFragmentRegistry.sPathToFragmentMap;
        Map<Class<? extends Fragment>, String> fragmentMap =
                SettingsFragmentRegistry.sFragmentToPathMap;

        // Lookup key should be lowercased
        assertTrue(pathMap.containsKey("/test/case"));
        assertEquals(Fragment.class, pathMap.get("/test/case"));

        // Canonical path map should preserve casing of the subpage path (without the slash)
        assertEquals("Test/Case", fragmentMap.get(Fragment.class));

        // Clean up
        pathMap.remove("/test/case");
        fragmentMap.remove(Fragment.class);
    }

    @Test
    public void testValidUrls() {
        assertEquals(
                AppearanceSettingsFragment.class,
                SettingsFragmentRegistry.getFragmentClassForUrl("chrome://settings/appearance"));
        assertEquals(
                ThemeSettingsFragment.class,
                SettingsFragmentRegistry.getFragmentClassForUrl("chrome://settings/theme"));
        assertEquals(
                MainSettings.class,
                SettingsFragmentRegistry.getFragmentClassForUrl("chrome://settings"));
        assertEquals(
                MainSettings.class,
                SettingsFragmentRegistry.getFragmentClassForUrl("chrome://settings/"));
    }

    @Test
    public void testCaseInsensitivePathLookup() {
        assertEquals(
                AppearanceSettingsFragment.class,
                SettingsFragmentRegistry.getFragmentClassForUrl("chrome://settings/APPEARANCE"));
        assertEquals(
                LegalInformationSettings.class,
                SettingsFragmentRegistry.getFragmentClassForUrl("chrome://settings/ABOUT/LEGAL"));
    }

    @Test
    public void testInvalidSchemesAndHostsRejected() {
        assertNull(
                SettingsFragmentRegistry.getFragmentClassForUrl("https://google.com/appearance"));
        assertNull(SettingsFragmentRegistry.getFragmentClassForUrl("file:///appearance"));
        assertNull(SettingsFragmentRegistry.getFragmentClassForUrl("chrome://history/appearance"));
    }

    @Test
    public void testTrailingSlashNormalization() {
        assertEquals(
                AppearanceSettingsFragment.class,
                SettingsFragmentRegistry.getFragmentClassForUrl("chrome://settings/appearance/"));
        assertEquals(
                LegalInformationSettings.class,
                SettingsFragmentRegistry.getFragmentClassForUrl("chrome://settings/about/legal/"));
    }

    @Test
    public void testParseUrlArguments() {
        assertTrue(SettingsFragmentRegistry.parseUrlArguments("").isEmpty());

        Bundle bundle =
                SettingsFragmentRegistry.parseUrlArguments(
                        "chrome://settings/siteDetails?site=example.com");
        assertEquals("example.com", bundle.getString(SingleWebsiteSettings.EXTRA_SITE_ADDRESS));

        // Non-hierarchical URIs return an empty bundle safely.
        assertTrue(SettingsFragmentRegistry.parseUrlArguments("mailto:user@example.com").isEmpty());
    }

    @Test
    public void testTypedQueryParameterParsing() {
        Bundle bundle =
                SettingsFragmentRegistry.parseUrlArguments(
                        "chrome://settings/foo?boolKey=true&intKey=42");
        assertEquals("true", bundle.getString("boolKey"));
        assertEquals("42", bundle.getString("intKey"));
    }

    @Test
    public void testCreateUrlForFragment() {
        Bundle args = new Bundle();
        args.putString(SingleWebsiteSettings.EXTRA_SITE_ADDRESS, "example.com");
        String url =
                SettingsFragmentRegistry.createUrlForFragment(SingleWebsiteSettings.class, args);
        assertEquals("chrome://settings/siteDetails?site=example.com", url);

        // Verify WebsiteAddress argument handling.
        Bundle addressArgs = new Bundle();
        addressArgs.putSerializable(
                SingleWebsiteSettings.EXTRA_SITE_ADDRESS,
                WebsiteAddress.create("https://example.com"));
        assertEquals(
                "chrome://settings/siteDetails?site=https%3A%2F%2Fexample.com",
                SettingsFragmentRegistry.createUrlForFragment(
                        SingleWebsiteSettings.class, addressArgs));

        // Verify Website argument handling.
        Bundle websiteArgs = new Bundle();
        websiteArgs.putSerializable(
                SingleWebsiteSettings.EXTRA_SITE_ADDRESS,
                new Website(WebsiteAddress.create("https://example.com"), null));
        assertEquals(
                "chrome://settings/siteDetails?site=https%3A%2F%2Fexample.com",
                SettingsFragmentRegistry.createUrlForFragment(
                        SingleWebsiteSettings.class, websiteArgs));

        // Verify WebsiteGroup argument handling.
        Bundle groupArgs = new Bundle();
        groupArgs.putSerializable(
                GroupedWebsitesSettings.EXTRA_GROUP,
                new WebsiteGroup("google.com", Collections.emptyList()));
        assertEquals(
                "chrome://settings/allSites/group?group=google.com",
                SettingsFragmentRegistry.createUrlForFragment(
                        GroupedWebsitesSettings.class, groupArgs));
    }

    @Test
    public void testParseUrlArgumentsPopulatesDefaultMandatoryExtras() {
        // Verify that parsing a theme settings URL without explicit query
        // parameters automatically populates the mandatory
        // theme_settings_entry extra expected by ThemeSettingsFragment.
        Bundle bundle = SettingsFragmentRegistry.parseUrlArguments("chrome://settings/theme");
        assertTrue(bundle.containsKey(ThemeSettingsFragment.KEY_THEME_SETTINGS_ENTRY));
        assertEquals(
                NightModeMetrics.ThemeSettingsEntry.SETTINGS,
                bundle.getInt(ThemeSettingsFragment.KEY_THEME_SETTINGS_ENTRY));
    }

    @Test
    public void testPreferenceNavigationUrlGeneration() {
        // Verify that root category and sublevel preference targets construct
        // canonical chrome://settings/ URLs.
        assertEquals(
                "chrome://settings/appearance",
                SettingsFragmentRegistry.createUrlForFragment(
                        AppearanceSettingsFragment.class, null));

        assertEquals(
                "chrome://settings/theme",
                SettingsFragmentRegistry.createUrlForFragment(ThemeSettingsFragment.class, null));

        assertEquals(
                "chrome://settings/privacy",
                SettingsFragmentRegistry.createUrlForFragment(PrivacySettings.class, null));

        assertEquals(
                "chrome://settings/clearBrowsingData",
                SettingsFragmentRegistry.createUrlForFragment(
                        ClearBrowsingDataFragment.class, null));

        assertEquals(
                "chrome://settings/safeBrowsing",
                SettingsFragmentRegistry.createUrlForFragment(
                        SafeBrowsingSettingsFragment.class, null));

        Bundle siteArgs = new Bundle();
        siteArgs.putString(SingleWebsiteSettings.EXTRA_SITE_ADDRESS, "https://example.com");
        assertEquals(
                "chrome://settings/siteDetails?site=https%3A%2F%2Fexample.com",
                SettingsFragmentRegistry.createUrlForFragment(
                        SingleWebsiteSettings.class, siteArgs));
    }

    @Test
    public void testHierarchicalUrlToFragmentResolution() {
        // Verify that deep link URLs properly resolve to their corresponding
        // root category and sublevel fragment classes.
        assertEquals(
                PreloadPagesSettingsFragment.class,
                SettingsFragmentRegistry.getFragmentClassForUrl("chrome://settings/preloadPages"));

        assertEquals(
                StandardPreloadingSettingsFragment.class,
                SettingsFragmentRegistry.getFragmentClassForUrl(
                        "chrome://settings/preloadPages/standard"));

        assertEquals(
                ExtendedPreloadingSettingsFragment.class,
                SettingsFragmentRegistry.getFragmentClassForUrl(
                        "chrome://settings/preloadPages/extended"));

        assertEquals(
                SafeBrowsingSettingsFragment.class,
                SettingsFragmentRegistry.getFragmentClassForUrl("chrome://settings/safeBrowsing"));

        assertEquals(
                StandardProtectionSettingsFragment.class,
                SettingsFragmentRegistry.getFragmentClassForUrl(
                        "chrome://settings/safeBrowsing/standard"));

        assertEquals(
                EnhancedProtectionSettingsFragment.class,
                SettingsFragmentRegistry.getFragmentClassForUrl(
                        "chrome://settings/safeBrowsing/enhanced"));

        assertEquals(
                AboutChromeSettings.class,
                SettingsFragmentRegistry.getFragmentClassForUrl("chrome://settings/about"));

        assertEquals(
                LegalInformationSettings.class,
                SettingsFragmentRegistry.getFragmentClassForUrl("chrome://settings/about/legal"));

        assertEquals(
                DeveloperSettings.class,
                SettingsFragmentRegistry.getFragmentClassForUrl("chrome://settings/developer"));

        assertEquals(
                TracingSettings.class,
                SettingsFragmentRegistry.getFragmentClassForUrl(
                        "chrome://settings/developer/tracing"));
    }
}
