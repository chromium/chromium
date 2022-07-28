// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.feedmanagement;

import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import android.view.View;
import android.view.ViewGroup;

import androidx.appcompat.widget.AppCompatImageButton;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.feed.FeedServiceBridge;
import org.chromium.chrome.browser.feed.FeedServiceBridgeJni;
import org.chromium.chrome.browser.feed.StreamKind;
import org.chromium.chrome.browser.feed.test.R;
import org.chromium.ui.base.TestActivity;

/**
 * Tests {@link FeedManagementCoordinator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class FeedManagementCoordinatorTest {
    private TestActivity mActivity;
    private FeedManagementCoordinator mFeedManagementCoordinator;

    @Rule
    public JniMocker mocker = new JniMocker();
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock
    private FeedServiceBridge.Natives mFeedServiceBridgeJniMock;

    @Before
    public void setUpTest() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);

        MockitoAnnotations.initMocks(this);
        mocker.mock(FeedServiceBridgeJni.TEST_HOOKS, mFeedServiceBridgeJniMock);

        mFeedManagementCoordinator =
                new FeedManagementCoordinator(mActivity, null, null, StreamKind.UNKNOWN);

        verify(mFeedServiceBridgeJniMock).isAutoplayEnabled();
    }

    @Test
    public void testConstruction() {
        assertTrue(true);
    }

    @Test
    public void testBackArrow() {
        View outerView = mFeedManagementCoordinator.getView();
        // Send a click to the back arrow.
        // Note that finding the back arrow view is ugly because it doesn't
        // have an ID.
        boolean clicked = false;
        ViewGroup actionBar = outerView.findViewById(R.id.action_bar);
        for (int i = 0; i < actionBar.getChildCount(); i++) {
            try {
                AppCompatImageButton button = (AppCompatImageButton) actionBar.getChildAt(i);
                button.performClick();
                clicked = true;
            } catch (ClassCastException e) {
            }
        }

        assertTrue(clicked);
    }
}
