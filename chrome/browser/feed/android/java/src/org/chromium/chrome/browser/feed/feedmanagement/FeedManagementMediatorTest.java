// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.feedmanagement;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.shadows.ShadowActivity;

import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.feed.FeedServiceBridge;
import org.chromium.chrome.browser.feed.FeedServiceBridgeJni;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

/**
 * Tests {@link FeedManagementMediator}.
 */
@RunWith(LocalRobolectricTestRunner.class)
public class FeedManagementMediatorTest {
    private Activity mActivity;
    private ShadowActivity mShadowActivity;
    private ModelList mModelList;
    private FeedManagementMediator mFeedManagementMediator;

    @Rule
    public JniMocker mocker = new JniMocker();

    @Mock
    private FeedServiceBridge.Natives mFeedServiceBridgeJniMock;

    @Mock
    private FeedManagementMediator.FollowManagementLauncher mFollowManagementLauncher;

    @Mock
    private FeedManagementMediator.AutoplayManagementLauncher mAutoplayManagementLauncher;

    @Before
    public void setUpTest() {
        mActivity = Robolectric.setupActivity(Activity.class);
        mShadowActivity = Shadows.shadowOf(mActivity);
        mModelList = new ModelList();
        MockitoAnnotations.initMocks(this);
        mocker.mock(FeedServiceBridgeJni.TEST_HOOKS, mFeedServiceBridgeJniMock);

        mFeedManagementMediator = new FeedManagementMediator(
                mActivity, mModelList, mFollowManagementLauncher, mAutoplayManagementLauncher);

        verify(mFeedServiceBridgeJniMock).isAutoplayEnabled();
    }

    @Test
    public void testHandleActivityClick() {
        // Act
        mFeedManagementMediator.handleActivityClick(null);

        // Assert
        Intent intent = mShadowActivity.peekNextStartedActivityForResult().intent;
        assertEquals(
                intent.getData(), Uri.parse("https://myactivity.google.com/myactivity?product=50"));
    }

    @Test
    public void testHandleInterestsClick() {
        // Act
        mFeedManagementMediator.handleInterestsClick(null);

        // Assert
        Intent intent = mShadowActivity.peekNextStartedActivityForResult().intent;
        assertEquals(intent.getData(),
                Uri.parse("https://www.google.com/preferences/interests/yourinterests?sh=n"));
    }

    @Test
    public void testHandleHiddenClick() {
        // Act
        mFeedManagementMediator.handleHiddenClick(null);

        // Assert
        Intent intent = mShadowActivity.peekNextStartedActivityForResult().intent;
        assertEquals(intent.getData(),
                Uri.parse("https://www.google.com/preferences/interests/hidden?sh=n"));
    }

    @Test
    public void testHandleFollowingClick() {
        // Act
        mFeedManagementMediator.handleFollowingClick(null);

        // Assert
        verify(mFollowManagementLauncher).launchFollowManagement(mActivity);
    }

    @Test
    public void testHandleAutoplayClick() {
        // Act
        mFeedManagementMediator.handleAutoplayClick(null);

        // Assert
        verify(mAutoplayManagementLauncher).launchAutoplayManagement(mActivity);
    }
}
