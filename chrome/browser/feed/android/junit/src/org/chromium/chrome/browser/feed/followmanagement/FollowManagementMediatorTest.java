// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.followmanagement;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;

import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.feed.webfeed.WebFeedAvailabilityStatus;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge.WebFeedMetadata;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridgeJni;
import org.chromium.chrome.browser.feed.webfeed.WebFeedSubscriptionStatus;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests {@link FollowManagementMediator}.
 */
@RunWith(LocalRobolectricTestRunner.class)
public class FollowManagementMediatorTest {
    private Activity mActivity;
    private ModelList mModelList;
    private FollowManagementMediator mFollowManagementMediator;
    private LargeIconBridge mLargeIconBridge;

    @Rule
    public JniMocker mocker = new JniMocker();

    @Mock
    WebFeedBridge.Natives mWebFeedBridgeJni;

    @Mock
    SimpleRecyclerViewAdapter mAdapter;

    // Stub LargeIconBridge that always returns false.
    private class TestLargeIconBridge extends LargeIconBridge {
        @Override
        public boolean getLargeIconForStringUrl(
                final String pageUrl, int desiredSizePx, final LargeIconCallback callback) {
            Bitmap bitmap = Bitmap.createBitmap(1 /* width */, 1 /* height */, Config.ARGB_8888);
            callback.onLargeIconAvailable(bitmap, 0 /* fallback color */,
                    true /* isFallbackColorDefault */, 0 /* icon type */);
            return false;
        }
        @Override
        public boolean getLargeIconForUrl(
                final GURL pageUrl, int desiredSizePx, final LargeIconCallback callback) {
            Bitmap bitmap = Bitmap.createBitmap(1 /* width */, 1 /* height */, Config.ARGB_8888);
            callback.onLargeIconAvailable(bitmap, 0 /* fallback color */,
                    true /* isFallbackColorDefault */, 0 /* icon type */);
            return true;
        }
    }

    @Before
    public void setUpTest() {
        mActivity = Robolectric.setupActivity(Activity.class);
        mModelList = new ModelList();
        MockitoAnnotations.initMocks(this);
        mLargeIconBridge = new TestLargeIconBridge();
        mocker.mock(WebFeedBridgeJni.TEST_HOOKS, mWebFeedBridgeJni);

        mFollowManagementMediator =
                new FollowManagementMediator(mActivity, mModelList, mAdapter, mLargeIconBridge);

        // WebFeedBridge.refreshFollowedWebFeeds() gets called once with non-null pointer to a
        // callback.
        verify(mWebFeedBridgeJni).refreshSubscriptions(notNull());
    }

    @Test
    public void testLoadingState() {
        // Loading state is set upon construction.
        ModelList modelList = mFollowManagementMediator.getModelListForTest();
        assertEquals(1, modelList.size());
        ListItem item = modelList.get(0);
        assertEquals(FollowManagementItemProperties.LOADING_ITEM_TYPE, item.type);
    }

    @Test
    public void testEmptyWebFeedList() {
        mFollowManagementMediator.fillRecyclerView(new ArrayList<WebFeedMetadata>());

        ModelList modelList = mFollowManagementMediator.getModelListForTest();

        // For an empty list of feeds, we should see the empty state item.
        assertEquals(1, modelList.size());
        ListItem item = modelList.get(0);
        assertEquals(FollowManagementItemProperties.EMPTY_ITEM_TYPE, item.type);
    }

    @Test
    public void testWebFeedList() {
        List<WebFeedMetadata> metadataList = new ArrayList<WebFeedMetadata>();
        byte[] id1 = new byte[] {(byte) 0x11, (byte) 0x11};
        byte[] id2 = new byte[] {(byte) 0x22, (byte) 0x22};
        GURL url1 = JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_1);
        GURL url2 = JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_2);
        metadataList.add(new WebFeedMetadata(id1, "Programmers at work", url1,
                WebFeedSubscriptionStatus.SUBSCRIBED, WebFeedAvailabilityStatus.ACTIVE, false));
        metadataList.add(new WebFeedMetadata(id1, "Programmers at play", url2,
                WebFeedSubscriptionStatus.NOT_SUBSCRIBED, WebFeedAvailabilityStatus.INACTIVE,
                false));

        mFollowManagementMediator.fillRecyclerView(metadataList);

        ModelList modelList = mFollowManagementMediator.getModelListForTest();

        // We should see two items in the list.
        assertEquals(2, modelList.size());

        // The item type should be DEFAULT_ITEM_TYPE.
        ListItem item = modelList.get(0);
        assertEquals(FollowManagementItemProperties.DEFAULT_ITEM_TYPE, item.type);
    }

    // Future tests we could write:
    // Test the click listener is set up properly.
    // Test that the favicon is being set properly asynchronously.
}
