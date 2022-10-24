// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.creator;

import static org.junit.Assert.assertNotNull;

import android.view.View;
import android.view.ViewGroup;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.creator.test.R;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge;
import org.chromium.ui.base.TestActivity;

/**
 * Tests for {@link CreatorCoordinator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class CreatorCoordinatorTest {
    @Mock
    private WebFeedBridge.Natives mWebFeedBridgeJniMock;
    @Mock
    private CreatorApiBridge.Natives mCreatorBridgeJniMock;

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);
    @Rule
    public JniMocker mJniMocker = new JniMocker();

    private TestActivity mActivity;
    private CreatorCoordinator mCreatorCoordinator;
    private static final byte[] sWebFeedId = "webFeedId".getBytes();

    @Before
    public void setUpTest() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(CreatorApiBridgeJni.TEST_HOOKS, mCreatorBridgeJniMock);
        mJniMocker.mock(WebFeedBridge.getTestHooksForTesting(), mWebFeedBridgeJniMock);

        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);

        mCreatorCoordinator = new CreatorCoordinator(mActivity, sWebFeedId);
    }

    @Test
    public void testCreatorCoordinatorConstruction() {
        assertNotNull("Could not construct CreatorCoordinator", mCreatorCoordinator);
    }

    @Test
    public void testActionBar() {
        View outerView = mCreatorCoordinator.getView();
        ViewGroup actionBar = (ViewGroup) outerView.findViewById(R.id.action_bar);
        assertNotNull("Could not retrieve ActionBar", actionBar);
    }
}
