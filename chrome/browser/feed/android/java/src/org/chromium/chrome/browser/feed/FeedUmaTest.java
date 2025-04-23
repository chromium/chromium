// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static org.mockito.Mockito.verify;

import android.app.Activity;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.feed.v2.FeedUserActionType;

/** Unit tests for {@link FeedUma}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FeedUmaTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private FeedServiceBridge.Natives mFeedServiceBridgeJniMock;

    @Before
    public void setUpTest() {
        Robolectric.setupActivity(Activity.class);
        FeedServiceBridgeJni.setInstanceForTesting(mFeedServiceBridgeJniMock);
    }

    @Test
    @SmallTest
    public void testRecordFeedBottomSheetItemsClicked() {
        // Verifies that the histogram is logged correctly when the feed is turned on and off.
        String articlesListVisibleHistogramName =
                "NewTabPage.ContentSuggestions.ArticlesListVisible";

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(articlesListVisibleHistogramName, true)
                        .build();
        FeedUma.recordFeedBottomSheetItemsClicked(FeedUserActionType.TAPPED_TURN_ON);
        histogramWatcher.assertExpected();
        verify(mFeedServiceBridgeJniMock).reportOtherUserAction(FeedUserActionType.TAPPED_TURN_ON);

        histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(articlesListVisibleHistogramName, false)
                        .build();
        FeedUma.recordFeedBottomSheetItemsClicked(FeedUserActionType.TAPPED_TURN_OFF);
        histogramWatcher.assertExpected();
        verify(mFeedServiceBridgeJniMock).reportOtherUserAction(FeedUserActionType.TAPPED_TURN_OFF);

        // Verifies that the histogram is logged correctly when different sections in the feed
        // bottom sheet are clicked.
        testRecordFeedBottomSheetItemsClickedImpl(FeedUserActionType.TAPPED_MANAGE_ACTIVITY);

        testRecordFeedBottomSheetItemsClickedImpl(FeedUserActionType.TAPPED_MANAGE_FOLLOWING);

        testRecordFeedBottomSheetItemsClickedImpl(FeedUserActionType.TAPPED_MANAGE_HIDDEN);

        testRecordFeedBottomSheetItemsClickedImpl(FeedUserActionType.TAPPED_MANAGE_INTERESTS);
    }

    private void testRecordFeedBottomSheetItemsClickedImpl(
            @FeedUserActionType int feedUserActionType) {
        FeedUma.recordFeedBottomSheetItemsClicked(feedUserActionType);
        verify(mFeedServiceBridgeJniMock).reportOtherUserAction(feedUserActionType);
    }
}
