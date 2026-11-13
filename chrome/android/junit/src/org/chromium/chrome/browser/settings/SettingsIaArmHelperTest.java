// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.res.XmlResourceParser;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.xmlpull.v1.XmlPullParser;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.settings.search.BaseSearchIndexProvider;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Unit tests for {@link SettingsIaArmHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SettingsIaArmHelperTest {
    private static final String MAIN_SETTINGS = MainSettings.class.getName();

    private Context mContext;
    private SettingsIndexData mIndexData;
    private BaseSearchIndexProvider mProvider;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        SettingsIndexData.reset();
        mIndexData = SettingsIndexData.createInstance();
        mProvider = new BaseSearchIndexProvider(MAIN_SETTINGS);
    }

    @After
    public void tearDown() {
        // The index is a singleton; don't leak it into other tests.
        SettingsIndexData.reset();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SETTINGS_IA_REORGANIZATION)
    public void testGetArm_defaultsToDirection1() {
        assertEquals(SettingsIaArmHelper.Arm.DIRECTION_1, SettingsIaArmHelper.getArm());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SETTINGS_IA_REORGANIZATION + ":arm/direction_2")
    public void testGetArm_direction2() {
        assertEquals(SettingsIaArmHelper.Arm.DIRECTION_2, SettingsIaArmHelper.getArm());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SETTINGS_IA_REORGANIZATION + ":arm/direction_3")
    public void testGetArm_direction3() {
        assertEquals(SettingsIaArmHelper.Arm.DIRECTION_3, SettingsIaArmHelper.getArm());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SETTINGS_IA_REORGANIZATION + ":arm/unknown")
    public void testGetArm_unknownFallsBackToDirection1() {
        assertEquals(SettingsIaArmHelper.Arm.DIRECTION_1, SettingsIaArmHelper.getArm());
    }

    @Test
    public void testGroups_inDisplayOrder() {
        List<SettingsIaArmHelper.Group> groups = SettingsIaArmHelper.getGroups();
        assertEquals(4, groups.size());
        assertEquals(SettingsIaArmHelper.GROUP_USER_AND_ACCOUNT, groups.get(0).id);
        assertEquals(SettingsIaArmHelper.GROUP_SAFETY_AND_PROTECTION, groups.get(1).id);
        assertEquals(SettingsIaArmHelper.GROUP_PERSONALIZATION, groups.get(2).id);
        assertEquals(SettingsIaArmHelper.GROUP_SUPPORT, groups.get(3).id);
    }

    @Test
    public void testGroups_eachRowInOneGroup() {
        Set<String> keys = new HashSet<>();
        for (SettingsIaArmHelper.Group group : SettingsIaArmHelper.getGroups()) {
            for (String key : group.keys) {
                assertTrue("Row is in more than one group: " + key, keys.add(key));
            }
        }
    }

    @Test
    public void testGroups_haveNamesAndRows() {
        Set<String> ids = new HashSet<>();
        for (SettingsIaArmHelper.Group group : SettingsIaArmHelper.getGroups()) {
            assertTrue("Duplicate group id: " + group.id, ids.add(group.id));
            assertFalse(mContext.getString(group.nameRes).isEmpty());
            assertFalse("Group has no rows: " + group.id, group.keys.isEmpty());
        }
    }

    @Test
    public void testGroups_coverEveryTopLevelRow() throws Exception {
        Set<String> groupedKeys = new HashSet<>();
        for (SettingsIaArmHelper.Group group : SettingsIaArmHelper.getGroups()) {
            groupedKeys.addAll(group.keys);
        }
        for (String key : getTopLevelRowKeys()) {
            assertTrue("Top-level row is not in a group: " + key, groupedKeys.contains(key));
        }
    }

    @Test
    public void testSetGroupHeaders_topLevelRowsUseGroupName() {
        addEntry(MainSettings.PREF_SIGN_IN, "You and Google");
        addEntry(MainSettings.PREF_PRIVACY, "Basics");
        addEntry(MainSettings.PREF_SEARCH_ENGINE, "Basics");
        addEntry(MainSettings.PREF_DOWNLOADS, "Advanced");

        SettingsIaArmHelper.setGroupHeaders(mContext, mIndexData, mProvider);

        assertHeader(MainSettings.PREF_SIGN_IN, R.string.prefs_group_user_and_account);
        assertHeader(MainSettings.PREF_PRIVACY, R.string.prefs_group_safety_and_protection);
        assertHeader(MainSettings.PREF_SEARCH_ENGINE, R.string.prefs_group_personalization);
        assertHeader(MainSettings.PREF_DOWNLOADS, R.string.prefs_group_support);
    }

    @Test
    public void testSetGroupHeaders_skipsMissingAndUngroupedRows() {
        addEntry(MainSettings.PREF_SETTINGS_PROMO_CARD, null);

        // Rows that are not indexed (e.g. hidden ones) are skipped without adding entries.
        SettingsIaArmHelper.setGroupHeaders(mContext, mIndexData, mProvider);

        assertNull(entry(MainSettings.PREF_PRIVACY));
        assertNull(entry(MainSettings.PREF_SETTINGS_PROMO_CARD).header);
    }

    /**
     * Returns the keys of the rows in main_preferences.xml, other than the category headers and the
     * promo card. Reads the XML so rows hidden in the test configuration are included.
     */
    private List<String> getTopLevelRowKeys() throws Exception {
        List<String> keys = new ArrayList<>();
        try (XmlResourceParser parser = mContext.getResources().getXml(R.xml.main_preferences)) {
            for (int event = parser.next();
                    event != XmlPullParser.END_DOCUMENT;
                    event = parser.next()) {
                if (event != XmlPullParser.START_TAG) continue;
                String key = getKeyAttribute(parser);
                if (key == null
                        || parser.getName().endsWith("Category")
                        || MainSettings.PREF_SETTINGS_PROMO_CARD.equals(key)) {
                    continue;
                }
                keys.add(key);
            }
        }
        assertFalse(keys.isEmpty());
        return keys;
    }

    private static @Nullable String getKeyAttribute(XmlResourceParser parser) {
        for (int i = 0; i < parser.getAttributeCount(); i++) {
            if (parser.getAttributeNameResource(i) == android.R.attr.key
                    || "key".equals(parser.getAttributeName(i))) {
                return parser.getAttributeValue(i);
            }
        }
        return null;
    }

    private void addEntry(String key, String header) {
        String id = mProvider.getUniqueId(key);
        mIndexData.addEntry(
                id,
                new SettingsIndexData.Entry.Builder(id, key, key, MAIN_SETTINGS)
                        .setHeader(header)
                        .build());
    }

    private SettingsIndexData.Entry entry(String key) {
        return mIndexData.getEntry(mProvider.getUniqueId(key));
    }

    private void assertHeader(String key, int nameRes) {
        assertEquals(mContext.getString(nameRes), entry(key).header);
    }
}
