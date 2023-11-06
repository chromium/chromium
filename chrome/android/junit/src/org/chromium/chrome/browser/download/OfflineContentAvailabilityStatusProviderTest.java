// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.OfflineItem;

import java.util.ArrayList;
import java.util.Arrays;

/** Unit tests for {@link OfflineContentAvailabilityStatusProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class OfflineContentAvailabilityStatusProviderTest {
    OfflineItem mTransientItem;
    OfflineItem mPersistentItem;
    OfflineItem mPrefetchItem;

    @Before
    public void setUp() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(
                        ChromePreferenceKeys.EXPLORE_OFFLINE_CONTENT_AVAILABILITY_STATUS, false);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(
                        ChromePreferenceKeys.PERSISTENT_OFFLINE_CONTENT_AVAILABILITY_STATUS, false);

        mTransientItem = new OfflineItem();
        mTransientItem.id = new ContentId(null, "0");
        mTransientItem.isTransient = true;
        mTransientItem.isSuggested = false;

        mPersistentItem = new OfflineItem();
        mPersistentItem.id = new ContentId(null, "1");
        mPersistentItem.isTransient = false;
        mPersistentItem.isSuggested = false;

        mPrefetchItem = new OfflineItem();
        mPrefetchItem.id = new ContentId(null, "2");
        mPrefetchItem.isTransient = false;
        mPrefetchItem.isSuggested = true;
    }

    @Test
    public void testIsPrefetchContentAvailable() {
        OfflineContentAvailabilityStatusProvider provider =
                new OfflineContentAvailabilityStatusProvider();
        assertFalse(
                "No prefetch content should be available initially.",
                provider.isSuggestedContentAvailable());

        // Add some non-prefetch items.
        provider.onItemsAdded(
                new ArrayList<OfflineItem>(Arrays.asList(mTransientItem, mPersistentItem)));
        assertFalse(
                "Added non-prefetch content should not affect prefetch content availability.",
                provider.isSuggestedContentAvailable());

        // Add a prefetch item.
        provider.onItemsAdded(new ArrayList<OfflineItem>(Arrays.asList(mPrefetchItem)));
        assertTrue(
                "Prefetch content should be available after adding prefetch content.",
                provider.isSuggestedContentAvailable());

        // Remove a non-prefetch item.
        provider.onItemRemoved(mPersistentItem.id);
        assertTrue(
                "Removed non-prefetch content should not affect prefetch content availability.",
                provider.isSuggestedContentAvailable());

        // Remove the prefetch item.
        provider.onItemRemoved(mPrefetchItem.id);
        assertFalse(
                "Prefetch content should not be available after removing all prefetch content.",
                provider.isSuggestedContentAvailable());
    }

    @Test
    public void testIsPersistentContentAvailable() {
        OfflineContentAvailabilityStatusProvider provider =
                new OfflineContentAvailabilityStatusProvider();
        assertFalse(
                "No persistent content should be available initially.",
                provider.isPersistentContentAvailable());

        // Add a transient item.
        provider.onItemsAdded(new ArrayList<OfflineItem>(Arrays.asList(mTransientItem)));
        assertFalse(
                "Added transient content should not affect persistent content availability.",
                provider.isSuggestedContentAvailable());

        // Add a persistent item.
        provider.onItemsAdded(new ArrayList<OfflineItem>(Arrays.asList(mPersistentItem)));
        assertTrue(
                "Persistent content should be available after adding persistent content.",
                provider.isPersistentContentAvailable());

        // Add a persistent prefetch item.
        provider.onItemsAdded(new ArrayList<OfflineItem>(Arrays.asList(mPrefetchItem)));
        assertTrue(
                "Persistent content should still be available after adding persistent prefetch "
                        + "content.",
                provider.isPersistentContentAvailable());

        // Remove the persistent item.
        provider.onItemRemoved(mPersistentItem.id);
        assertTrue(
                "Persistent content should still be available after removing one of the two "
                        + "persistent items.",
                provider.isPersistentContentAvailable());

        // Remove the persistent prefetch item.
        provider.onItemRemoved(mPrefetchItem.id);
        assertFalse(
                "Persistent content should not be available after removing all persistent content.",
                provider.isPersistentContentAvailable());
    }
}
