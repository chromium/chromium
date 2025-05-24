// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;

import android.annotation.SuppressLint;
import android.content.Intent;
import android.os.Bundle;
import android.os.Parcel;

import androidx.annotation.ColorInt;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

import java.util.AbstractMap;
import java.util.ArrayList;
import java.util.List;
import java.util.Map.Entry;

/** Tests for {@link TabGroupMetadata}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({ChromeFeatureList.TAB_STRIP_GROUP_DRAG_DROP_ANDROID})
public class TabGroupMetadataUnitTest {

    private static final ArrayList<Entry<Integer, String>> TAB_IDS_TO_URLS =
            new ArrayList<>(
                    List.of(
                            new AbstractMap.SimpleEntry<>(1, "https://www.amazon.com"),
                            new AbstractMap.SimpleEntry<>(2, "https://www.youtube.com"),
                            new AbstractMap.SimpleEntry<>(3, "https://www.facebook.com")));
    private static final Token TAB_GROUP_ID = new Token(2L, 2L);
    private static final int ROOT_ID = 1;
    private static final int SELECTED_TAB_ID = 2;
    private static final int SOURCE_WINDOW_INDEX = 5;
    private static final @ColorInt int TAB_GROUP_COLOR = 0;
    private static final String TAB_GROUP_TITLE = "Title";
    private static final String MHTML_TAB_TITLE = "mhtml tab";
    private static final boolean TAB_GROUP_COLLAPSED = true;
    private static final boolean IS_GROUP_SHARED = false;
    private static final boolean IS_INCOGNITO = false;

    private final Intent mIntent = new Intent();
    private TabGroupMetadata mTabGroupMetadata;

    @Before
    public void setup() {
        mTabGroupMetadata =
                new TabGroupMetadata(
                        ROOT_ID,
                        SELECTED_TAB_ID,
                        SOURCE_WINDOW_INDEX,
                        TAB_GROUP_ID,
                        TAB_IDS_TO_URLS,
                        TAB_GROUP_COLOR,
                        TAB_GROUP_TITLE,
                        MHTML_TAB_TITLE,
                        TAB_GROUP_COLLAPSED,
                        IS_GROUP_SHARED,
                        IS_INCOGNITO);
    }

    @Test
    public void testTabGroupMetadataIntentPersistence_SerializeAndDeserialize() {
        // Store the metadata in the intent through serialization.
        IntentHandler.setTabGroupMetadata(mIntent, mTabGroupMetadata);

        // Get the deserialized metadata object from intent.
        TabGroupMetadata deserializedMetadata = IntentHandler.getTabGroupMetadata(mIntent);

        // Assert that the deserialized metadata matches the original to confirm successful
        // deserialization.
        assertEquals(
                "The metadata should be the same as the original metadata",
                mTabGroupMetadata,
                deserializedMetadata);
    }

    /** See {@link TabGroupMetadata#tabIdsToUrls} for more context. */
    @Test
    public void testTabIdsToUrlsOrder() {
        // Write the metadata Bundle to a Parcel.
        Parcel parcel = Parcel.obtain();
        try {
            // Serialize the metadata.
            parcel.writeBundle(mTabGroupMetadata.toBundle());

            // Read back the metadata.
            parcel.setDataPosition(0);
            Bundle bundle = parcel.readBundle(getClass().getClassLoader());
            assert bundle != null;
            @SuppressLint("VisibleForTests")
            ArrayList<?> deserializedList =
                    (ArrayList<?>) bundle.getSerializable(TabGroupMetadata.KEY_TAB_IDS_TO_URLS);

            // Verify the ordering is retained.
            assert deserializedList != null;
            for (int i = 0; i < TAB_IDS_TO_URLS.size(); i++) {
                assertEquals(
                        "Unexpected ordering after deserialization.",
                        TAB_IDS_TO_URLS.get(i),
                        deserializedList.get(i));
            }
        } finally {
            parcel.recycle();
        }
    }
}
