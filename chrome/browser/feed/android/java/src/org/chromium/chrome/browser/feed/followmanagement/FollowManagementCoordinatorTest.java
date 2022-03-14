// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.followmanagement;

import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.verify;

import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;

import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.AppCompatImageButton;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;

import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.feed.test.R;
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
    private TestActivity mActivity;
    private FollowManagementCoordinator mFollowManagementCoordinator;

    @Rule
    public JniMocker mocker = new JniMocker();

    @Mock
    LargeIconBridge.Natives mLargeIconBridgeJni;

    @Mock
    WebFeedBridge.Natives mWebFeedBridgeJni;

    @Mock
    private Profile mProfile;

    private static class TestActivity extends AppCompatActivity {
        TestActivity() {}
        @Override
        public void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            setTheme(R.style.Theme_BrowserUI);
        }
    }

    @Before
    public void setUpTest() {
        mActivity = Robolectric.setupActivity(TestActivity.class);

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
        // Send a click to the back arrow.
        // Note that finding the back arrow view is ugly because it doesn't
        // have an ID.
        boolean clicked = false;
        ViewGroup actionBar = (ViewGroup) outerView.findViewById(R.id.action_bar);
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
