// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.followmanagement;

import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.verify;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridgeJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridgeJni;
import org.chromium.ui.base.TestActivity;

/** Tests {@link FollowManagementCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FollowManagementCoordinatorTest {
    private TestActivity mActivity;
    private FollowManagementCoordinator mFollowManagementCoordinator;

    @Rule public JniMocker mocker = new JniMocker();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock LargeIconBridge.Natives mLargeIconBridgeJni;

    @Mock WebFeedBridge.Natives mWebFeedBridgeJni;

    @Mock private Profile mProfile;

    @Before
    public void setUpTest() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);

        MockitoAnnotations.initMocks(this);
        ProfileManager.setLastUsedProfileForTesting(mProfile);
        mocker.mock(WebFeedBridgeJni.TEST_HOOKS, mWebFeedBridgeJni);
        mocker.mock(LargeIconBridgeJni.TEST_HOOKS, mLargeIconBridgeJni);

        mFollowManagementCoordinator = new FollowManagementCoordinator(mActivity);

        // WebFeedBridge.refreshFollowedWebFeeds() gets called once with non-null pointer to a
        // callback.
        verify(mWebFeedBridgeJni).refreshSubscriptions(notNull());
    }

    @Test
    public void testConstruction() {
        assertTrue(true);
    }
}
