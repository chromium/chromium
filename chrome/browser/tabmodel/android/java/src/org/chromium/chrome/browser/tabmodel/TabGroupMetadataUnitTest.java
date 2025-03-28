// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertTrue;

import android.content.Intent;

import androidx.annotation.ColorInt;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

import java.util.LinkedHashMap;
import java.util.Map;

/** Tests for {@link TabGroupMetadata}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({ChromeFeatureList.TAB_STRIP_GROUP_DRAG_DROP_ANDROID})
public class TabGroupMetadataUnitTest {

    private static final LinkedHashMap<Integer, String> TAB_IDS_TO_URLS =
            new LinkedHashMap<>(
                    Map.ofEntries(
                            Map.entry(1, "https://www.amazon.com"),
                            Map.entry(2, "https://www.youtube.com"),
                            Map.entry(3, "https://www.facebook.com")));
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
    private Intent mIntent = new Intent();
    private TabGroupMetadata mTabGroupMetadata;

    @Test
    public void testTabGroupMetadataIntentPersistence_SerializeAndDeserialize() {
        // Initialize the metadata object and store it in the intent through serialization.
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
        IntentHandler.setTabGroupMetadata(mIntent, mTabGroupMetadata);

        // Get the deserialized metadata object from intent.
        TabGroupMetadata deserializedMetadata = IntentHandler.getTabGroupMetadata(mIntent);

        // Assert that the deserialized metadata matches the original to confirm successful
        // deserialization.
        assertTrue(
                "The metadata should be the same as the original metadata",
                mTabGroupMetadata.equals(deserializedMetadata));
    }
}
