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
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.appearance.settings.AppearanceSettingsFragment;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

import java.util.Map;

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

    private BaseSearchIndexProvider mAppearanceSettingsProvider;

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();
        mIndexData = new SettingsIndexData();

        mAppearanceSettingsProvider =
                new BaseSearchIndexProvider(
                        AppearanceSettingsFragment.class.getName(), R.xml.appearance_preferences);
    }

    @Test
    public void testInitPreferenceXml_addsEntriesFromXml() {
        mAppearanceSettingsProvider.initPreferenceXml(mContext, mIndexData);

        Map<String, SettingsIndexData.Entry> entries = mIndexData.getEntriesForTesting();
        assertFalse("Index should not be empty after indexing.", entries.isEmpty());

        final String toolbarShortcutKey = "toolbar_shortcut";
        SettingsIndexData.Entry shortcutEntry = entries.get(toolbarShortcutKey);
        assertNotNull("'toolbar_shortcut' should be indexed.", shortcutEntry);
        assertEquals("Key should match the preference key.", toolbarShortcutKey, shortcutEntry.key);
        assertEquals(
                "Title should match the string resource.",
                mContext.getString(R.string.toolbar_shortcut),
                shortcutEntry.title);
        assertEquals(
                "Parent fragment should be AppearanceSettingsFragment.",
                AppearanceSettingsFragment.class.getName(),
                shortcutEntry.parentFragment);
    }

    @Test
    public void testInitPreferenceXml_doesNotIndexIfDisabled() {
        mIndexData.setDisabledFragment(AppearanceSettingsFragment.class.getName());

        mAppearanceSettingsProvider.initPreferenceXml(mContext, mIndexData);

        assertTrue(
                "Disabled fragment should not be indexed.",
                mIndexData.getEntriesForTesting().isEmpty());
    }

    @Test
    public void testInitPreferenceXml_providerWithNoXml_doesNothing() {
        BaseSearchIndexProvider providerWithNoXml =
                new BaseSearchIndexProvider("some.fragment.Name");

        providerWithNoXml.initPreferenceXml(mContext, mIndexData);

        assertTrue(
                "Provider with no XML resource should not add any entries.",
                mIndexData.getEntriesForTesting().isEmpty());
    }

    @Test
    @DisableFeatures({ChromeFeatureList.SEARCH_IN_SETTINGS})
    public void testInitPreferenceXml_whenFeatureDisabled_returnsNoEntries() {
        mAppearanceSettingsProvider.initPreferenceXml(mContext, mIndexData);

        assertTrue(
                "When SearchInSettings is disabled, no preferences should be indexed.",
                mIndexData.getEntriesForTesting().isEmpty());
    }
}
