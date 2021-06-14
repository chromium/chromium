// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.followmanagement;

import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.verify;

import android.app.Activity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;

import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridgeJni;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

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
            return false;
        }
    }

    @Before
    public void setUpTest() {
        mActivity = Robolectric.setupActivity(Activity.class);
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
    public void testConstruction() {
        assertTrue(true);
    }
}
