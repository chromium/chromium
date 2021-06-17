// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.followmanagement;

import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.View;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;

import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.feed.webfeed.R;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridgeJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridgeJni;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/**
 * Tests {@link FollowManagementCoordinator}.
 */
@RunWith(LocalRobolectricTestRunner.class)
public class FollowManagementCoordinatorTest {
    private Activity mActivity;
    private FollowManagementCoordinator mFollowManagementCoordinator;

    @Rule
    public JniMocker mocker = new JniMocker();

    @Mock
    LargeIconBridge.Natives mLargeIconBridgeJni;

    @Mock
    WebFeedBridge.Natives mWebFeedBridgeJni;

    @Mock
    private Profile mProfile;

    @Before
    public void setUpTest() {
        mActivity = Robolectric.setupActivity(Activity.class);
        MockitoAnnotations.initMocks(this);
        Profile.setLastUsedProfileForTesting(mProfile);
        mocker.mock(WebFeedBridgeJni.TEST_HOOKS, mWebFeedBridgeJni);
        mocker.mock(LargeIconBridgeJni.TEST_HOOKS, mLargeIconBridgeJni);

        mFollowManagementCoordinator = new FollowManagementCoordinator(mActivity);

        // WebFeedBridge.refreshFollowedWebFeeds() gets called once with non-null pointer to a
        // callback.
        verify(mWebFeedBridgeJni).refreshSubscriptions(notNull());
    }

    @After
    public void tearDown() {
        Profile.setLastUsedProfileForTesting(null);
    }

    @Test
    public void testConstruction() {
        assertTrue(true);
    }

    @Test
    public void testBackArrow() {
        View outerView = mFollowManagementCoordinator.getView();
        View backArrowView = outerView.findViewById(R.id.follow_management_back_arrow);

        // Send a click to the back arrow.
        backArrowView.performClick();
    }
}
