// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import androidx.fragment.app.Fragment;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.appearance.settings.AppearanceSettingsFragment;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.night_mode.settings.ThemeSettingsFragment;
import org.chromium.chrome.browser.privacy.settings.PrivacySettings;
import org.chromium.chrome.browser.safe_browsing.settings.SafeBrowsingSettingsFragment;
import org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings;

import java.util.Map;

/** Unit tests for {@link SettingsFragmentRegistry}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, qualifiers = "sw600dp")
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
}
