// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.feedmanagement;

import static org.junit.Assert.assertTrue;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.feed.FeedServiceBridge;
import org.chromium.chrome.browser.feed.FeedServiceBridgeJni;
import org.chromium.chrome.browser.feed.StreamKind;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.base.TestActivity;

/** Tests {@link FeedManagementCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.FEED_FOLLOW_UI_UPDATE)
public class FeedManagementCoordinatorTest {
    private TestActivity mActivity;
    private FeedManagementCoordinator mFeedManagementCoordinator;

    @Rule public JniMocker mocker = new JniMocker();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private FeedServiceBridge.Natives mFeedServiceBridgeJniMock;

    @Before
    public void setUpTest() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);

        MockitoAnnotations.initMocks(this);
        mocker.mock(FeedServiceBridgeJni.TEST_HOOKS, mFeedServiceBridgeJniMock);

        mFeedManagementCoordinator = new FeedManagementCoordinator(mActivity, StreamKind.UNKNOWN);
    }

    @Test
    public void testConstruction() {
        assertTrue(true);
    }
}
