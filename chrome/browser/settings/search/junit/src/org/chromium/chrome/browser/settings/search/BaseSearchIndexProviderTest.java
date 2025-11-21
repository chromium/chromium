// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.search;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.appearance.settings.AppearanceSettingsFragment;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.night_mode.settings.ThemeSettingsFragment;
import org.chromium.chrome.browser.settings.MainSettings;

import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Unit tests for {@link BaseSearchIndexProvider}.
 *
 * <p>These tests validate the core architectural behavior of the default search provider.
 */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.SEARCH_IN_SETTINGS})
public class BaseSearchIndexProviderTest {

    private Context mContext;
    private SettingsIndexData mIndexData;

    private BaseSearchIndexProvider mMainSettingsProvider;
    private BaseSearchIndexProvider mAppearanceSettingsProvider;
    private BaseSearchIndexProvider mThemeSettingsProvider;

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();
        mIndexData = new SettingsIndexData();

        mMainSettingsProvider =
                new BaseSearchIndexProvider(MainSettings.class.getName(), R.xml.main_preferences);
        mAppearanceSettingsProvider =
                new BaseSearchIndexProvider(
                        AppearanceSettingsFragment.class.getName(), R.xml.appearance_preferences);
        mThemeSettingsProvider =
                new BaseSearchIndexProvider(
                        ThemeSettingsFragment.class.getName(),
                        org.chromium.chrome.browser.night_mode.R.xml.theme_preferences);
    }

    @Test
    public void testRegisterFragmentHeaders_buildsParentMapRecursively() {
        Map<String, SearchIndexProvider> providerMap = new HashMap<>();
        providerMap.put(mMainSettingsProvider.getPrefFragmentName(), mMainSettingsProvider);
        providerMap.put(
                mAppearanceSettingsProvider.getPrefFragmentName(), mAppearanceSettingsProvider);
        providerMap.put(mThemeSettingsProvider.getPrefFragmentName(), mThemeSettingsProvider);
        Set<String> processedFragments = new HashSet<>();

        mMainSettingsProvider.registerFragmentHeaders(
                mContext, mIndexData, providerMap, processedFragments);

        Map<String, List<String>> parentMap = mIndexData.getChildFragmentToParentKeysForTesting();
        assertFalse("Parent map should not be empty after registration.", parentMap.isEmpty());

        String appearanceFragmentName = AppearanceSettingsFragment.class.getName();
        String expectedAppearanceParentKey =
                PreferenceParser.createUniqueId(MainSettings.class.getName(), "appearance");

        assertTrue(
                "Map should contain a link to AppearanceSettingsFragment.",
                parentMap.containsKey(appearanceFragmentName));
        assertEquals(
                "AppearanceSettings' parent preference should be 'appearance'.",
                expectedAppearanceParentKey,
                parentMap.get(appearanceFragmentName).get(0));

        String themeFragmentName = ThemeSettingsFragment.class.getName();
        String expectedThemeParentKey =
                PreferenceParser.createUniqueId(
                        AppearanceSettingsFragment.class.getName(), "ui_theme");

        assertTrue(
                "Map should contain a link to the grandchild ThemeSettingsFragment.",
                parentMap.containsKey(themeFragmentName));
        assertEquals(
                "ThemeSettings' parent preference should be 'ui_theme'.",
                expectedThemeParentKey,
                parentMap.get(themeFragmentName).get(0));
    }

    @Test
    public void testInitPreferenceXml_addsEntriesFromXml() {
        mAppearanceSettingsProvider.initPreferenceXml(mContext, mIndexData);

        Map<String, SettingsIndexData.Entry> entries = mIndexData.getEntriesForTesting();
        assertFalse("Index should not be empty after indexing.", entries.isEmpty());

        final String originalThemeKey = "ui_theme";
        final String uniqueThemeKey =
                PreferenceParser.createUniqueId(
                        AppearanceSettingsFragment.class.getName(), originalThemeKey);

        SettingsIndexData.Entry themeEntry = entries.get(uniqueThemeKey);
        assertNotNull("'ui_theme' should be indexed from appearance_preferences.", themeEntry);
        assertEquals("Key should match the preference key.", originalThemeKey, themeEntry.key);
        assertEquals(
                "Title should match the string resource.",
                mContext.getString(R.string.theme_settings),
                themeEntry.title);
        assertEquals(
                "Parent fragment name should be correctly set to the provider's fragment.",
                AppearanceSettingsFragment.class.getName(),
                themeEntry.parentFragment);
    }

    @Test
    public void testProviderWithNoXml_doesNothing() {
        BaseSearchIndexProvider providerWithNoXml =
                new BaseSearchIndexProvider("some.fragment.Name");
        Map<String, SearchIndexProvider> providerMap = new HashMap<>();
        Set<String> processedFragments = new HashSet<>();

        providerWithNoXml.initPreferenceXml(mContext, mIndexData);
        assertTrue(
                "Provider with no XML should not add any entries.",
                mIndexData.getEntriesForTesting().isEmpty());

        providerWithNoXml.registerFragmentHeaders(
                mContext, mIndexData, providerMap, processedFragments);
        assertTrue(
                "Provider with no XML should not add any parent-child links.",
                mIndexData.getChildFragmentToParentKeysForTesting().isEmpty());
    }
}
