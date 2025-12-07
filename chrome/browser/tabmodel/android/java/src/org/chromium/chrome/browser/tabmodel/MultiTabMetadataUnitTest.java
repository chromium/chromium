// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import android.content.Intent;
import android.os.Bundle;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.IntentHandler;

import java.util.ArrayList;
import java.util.List;

/** Tests for {@link MultiTabMetadata}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MultiTabMetadataUnitTest {

    private static final ArrayList<Integer> TAB_IDS = new ArrayList<>(List.of(1, 2, 3));
    private static final ArrayList<String> URLS =
            new ArrayList<>(
                    List.of(
                            "https://www.amazon.com",
                            "https://www.youtube.com",
                            "https://www.facebook.com"));
    private static final boolean[] IS_PINNED = new boolean[] {true, false, true};
    private static final boolean IS_INCOGNITO = false;

    private final Intent mIntent = new Intent();
    private MultiTabMetadata mMultiTabMetadata;

    @Before
    public void setup() {
        mMultiTabMetadata =
                MultiTabMetadata.createForTesting(TAB_IDS, URLS, IS_PINNED, IS_INCOGNITO);
    }

    @Test
    public void testMultiTabMetadataIntentPersistence_SerializeAndDeserialize() {
        // Store the metadata in the intent through serialization.
        IntentHandler.setMultiTabMetadata(mIntent, mMultiTabMetadata);

        // Get the deserialized metadata object from intent.
        MultiTabMetadata deserializedMetadata = IntentHandler.getMultiTabMetadata(mIntent);

        // Assert that the deserialized metadata matches the original to confirm successful
        // deserialization.
        assertEquals(
                "The metadata should be the same as the original metadata",
                mMultiTabMetadata,
                deserializedMetadata);
    }

    @Test
    public void testMaybeCreateFromBundle_NullBundle() {
        assertNull(
                "maybeCreateFromBundle should return null for a null bundle.",
                MultiTabMetadata.maybeCreateFromBundle(null));
    }

    @Test
    public void testMaybeCreateFromBundle_MissingTabIds() {
        Bundle bundle = mMultiTabMetadata.toBundle();
        bundle.remove(MultiTabMetadata.getTabIdsKeyForTesting());
        assertNull(
                "maybeCreateFromBundle should return null for a bundle with missing tab IDs.",
                MultiTabMetadata.maybeCreateFromBundle(bundle));
    }

    @Test
    public void testMaybeCreateFromBundle_MissingUrls() {
        Bundle bundle = mMultiTabMetadata.toBundle();
        bundle.remove(MultiTabMetadata.getTabUrlsKeyForTesting());
        assertNull(
                "maybeCreateFromBundle should return null for a bundle with missing URLs.",
                MultiTabMetadata.maybeCreateFromBundle(bundle));
    }

    @Test
    public void testMaybeCreateFromBundle_MissingIsPinned() {
        Bundle bundle = mMultiTabMetadata.toBundle();
        bundle.remove(MultiTabMetadata.getIsPinnedKeyForTesting());
        assertNull(
                "maybeCreateFromBundle should return null for a bundle with missing isPinned.",
                MultiTabMetadata.maybeCreateFromBundle(bundle));
    }

    @Test
    public void testMaybeCreateFromBundle_MissingIsIncognito() {
        Bundle bundle = mMultiTabMetadata.toBundle();
        bundle.remove(MultiTabMetadata.getIsIncognitoKeyForTesting());
        assertNull(
                "maybeCreateFromBundle should return null for a bundle with missing isIncognito.",
                MultiTabMetadata.maybeCreateFromBundle(bundle));
    }

    @Test
    public void testMaybeCreateFromBundle_MismatchedSizes() {
        ArrayList<Integer> tabIds = new ArrayList<>(List.of(1, 2));
        ArrayList<String> urls =
                new ArrayList<>(
                        List.of(
                                "https://www.amazon.com",
                                "https://www.youtube.com",
                                "https://www.facebook.com"));
        boolean[] isPinned = new boolean[] {true, false};
        MultiTabMetadata multiTabMetadata =
                MultiTabMetadata.createForTesting(tabIds, urls, isPinned, IS_INCOGNITO);
        Bundle bundle = multiTabMetadata.toBundle();
        assertNull(
                "maybeCreateFromBundle should return null for a bundle with mismatched sizes.",
                MultiTabMetadata.maybeCreateFromBundle(bundle));
    }
}
