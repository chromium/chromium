// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.Context;

import org.junit.After;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Unit tests for {@link TabCollectionMigrationUtil}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabCollectionMigrationUtilTest {
    private static final String FILENAME_1 = "tabs-1.bin";
    private static final String FILENAME_2 = "tabs-2.bin";

    @After
    public void tearDown() {
        ContextUtils.getApplicationContext()
                .getSharedPreferences(
                        "tab_collection_migration_util_shared_prefs", Context.MODE_PRIVATE)
                .edit()
                .clear()
                .apply();
    }

    @Test
    @EnableFeatures({ChromeFeatureList.TAB_COLLECTION_ANDROID})
    public void testSetAndGet_featureEnabled() {
        assertFalse(
                "Value should be false by default.",
                TabCollectionMigrationUtil.wasTabCollectionsActiveForMetadataFile(FILENAME_1));

        TabCollectionMigrationUtil.setTabCollectionsActiveForMetadataFile(FILENAME_1);

        assertTrue(
                "Value should be true after setting with feature enabled.",
                TabCollectionMigrationUtil.wasTabCollectionsActiveForMetadataFile(FILENAME_1));
    }

    @Test
    // TODO(crbug.com/454344854): Delete this test as part of feature cleanup as back migration
    // will no longer be possible.
    @DisableFeatures({ChromeFeatureList.TAB_COLLECTION_ANDROID})
    public void testSetAndGet_featureDisabled() {
        // Set to true explicitly to ensure the new value is written.
        ContextUtils.getApplicationContext()
                .getSharedPreferences(
                        "tab_collection_migration_util_shared_prefs", Context.MODE_PRIVATE)
                .edit()
                .putBoolean(FILENAME_1, true)
                .apply();
        assertTrue(
                "Value should be true initially.",
                TabCollectionMigrationUtil.wasTabCollectionsActiveForMetadataFile(FILENAME_1));

        TabCollectionMigrationUtil.setTabCollectionsActiveForMetadataFile(FILENAME_1);

        assertFalse(
                "Value should be false after setting with feature disabled.",
                TabCollectionMigrationUtil.wasTabCollectionsActiveForMetadataFile(FILENAME_1));
    }

    @Test
    public void testGet_defaultValue() {
        assertFalse(
                "Value should be false by default.",
                TabCollectionMigrationUtil.wasTabCollectionsActiveForMetadataFile(FILENAME_1));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.TAB_COLLECTION_ANDROID})
    public void testSetAndGet_multipleFiles() {
        assertFalse(
                "Value should be false by default for FILENAME_1.",
                TabCollectionMigrationUtil.wasTabCollectionsActiveForMetadataFile(FILENAME_1));
        assertFalse(
                "Value should be false by default for FILENAME_2.",
                TabCollectionMigrationUtil.wasTabCollectionsActiveForMetadataFile(FILENAME_2));

        TabCollectionMigrationUtil.setTabCollectionsActiveForMetadataFile(FILENAME_1);

        assertTrue(
                "Value should be true for FILENAME_1.",
                TabCollectionMigrationUtil.wasTabCollectionsActiveForMetadataFile(FILENAME_1));
        assertFalse(
                "Value should remain false for FILENAME_2.",
                TabCollectionMigrationUtil.wasTabCollectionsActiveForMetadataFile(FILENAME_2));
    }
}
