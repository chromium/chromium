// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.search;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.os.Bundle;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.privacy.settings.PrivacySettings;
import org.chromium.chrome.browser.settings.MainSettings;

import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** Unit tests for {@link PreferenceParser}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.SEARCH_IN_SETTINGS})
public class PreferenceParserTest {

    private Context mContext;

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();
    }

    @Test
    public void testParsePreferences_parsesBasicAttributesCorrectly() throws Exception {
        List<Bundle> parsedMetadata =
                PreferenceParser.parsePreferences(mContext, R.xml.main_preferences);
        assertNotNull("The parsed metadata should not be null.", parsedMetadata);
        assertTrue(
                "Should have parsed a reasonable number of preferences.",
                parsedMetadata.size() > 10);

        @Nullable Bundle privacyBundle = findBundleByKey(parsedMetadata, "privacy");
        assertNotNull("The 'privacy' preference should be found.", privacyBundle);

        // Verify the basic attributes are correctly parsed.
        assertEquals(
                mContext.getString(R.string.prefs_privacy_security),
                privacyBundle.getString(PreferenceParser.METADATA_TITLE));
        assertEquals(
                PrivacySettings.class.getName(),
                privacyBundle.getString(PreferenceParser.METADATA_FRAGMENT));
    }

    @Test
    public void testParsePreferences_handlesPreferenceWithNoFragment() throws Exception {
        List<Bundle> parsedMetadata =
                PreferenceParser.parsePreferences(mContext, R.xml.main_preferences);
        @Nullable Bundle notificationsBundle = findBundleByKey(parsedMetadata, "notifications");
        assertNotNull("The 'notifications' preference should be found.", notificationsBundle);

        assertNull(
                "The 'notifications' preference should not have a fragment attribute.",
                notificationsBundle.getString(PreferenceParser.METADATA_FRAGMENT));
    }

    @Test
    public void testParseAndRegisterHeaders_addsParentLinks() {
        SettingsIndexData indexData = new SettingsIndexData();
        Map<String, SearchIndexProvider> providerMap = new HashMap<>();
        providerMap.put(
                PrivacySettings.class.getName(),
                new BaseSearchIndexProvider(
                        PrivacySettings.class.getName(), R.xml.privacy_preferences));
        Set<String> processedFragments = new HashSet<>();

        PreferenceParser.parseAndRegisterHeaders(
                mContext,
                R.xml.main_preferences,
                MainSettings.class.getName(),
                indexData,
                providerMap,
                processedFragments);

        Map<String, List<String>> parentMap = indexData.getChildFragmentToParentKeysForTesting();
        assertFalse("The parent-child map should not be empty after parsing.", parentMap.isEmpty());

        String privacyFragmentName = PrivacySettings.class.getName();
        assertTrue(
                "Map should contain an entry for PrivacySettings.",
                parentMap.containsKey(privacyFragmentName));

        List<String> privacyParents = parentMap.get(privacyFragmentName);
        assertEquals(
                "PrivacySettings should have one parent in this context.",
                1,
                privacyParents.size());
        assertEquals(
                "The parent of PrivacySettings should be the 'privacy' preference.",
                PreferenceParser.createUniqueId(MainSettings.class.getName(), "privacy"),
                privacyParents.get(0));

        assertTrue(
                "The parsed fragment should be marked as processed.",
                processedFragments.contains(MainSettings.class.getName()));
    }

    @Nullable
    private Bundle findBundleByKey(List<Bundle> metadata, String key) {
        for (Bundle bundle : metadata) {
            if (key.equals(bundle.getString(PreferenceParser.METADATA_KEY))) {
                return bundle;
            }
        }
        return null;
    }
}
