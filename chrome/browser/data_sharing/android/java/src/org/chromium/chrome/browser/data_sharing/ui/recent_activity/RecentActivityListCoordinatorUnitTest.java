// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.recent_activity;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.verify;

import android.app.Activity;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.data_sharing.ui.recent_activity.RecentActivityListCoordinator.AvatarProvider;
import org.chromium.chrome.browser.data_sharing.ui.recent_activity.RecentActivityListCoordinator.FaviconProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.collaboration.messaging.MessagingBackendService;
import org.chromium.ui.base.TestActivity;

@RunWith(BaseRobolectricTestRunner.class)
public class RecentActivityListCoordinatorUnitTest {
    private static final String TEST_COLLABORATION_ID1 = "collaboration1";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Captor private ArgumentCaptor<EmptyBottomSheetObserver> mBottomSheetObserverCaptor;

    @Mock private MessagingBackendService mMessagingBackendService;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private FaviconProvider mFaviconProvider;
    @Mock private AvatarProvider mAvatarProvider;
    @Mock private RecentActivityActionHandler mRecentActivityActionHandler;
    private Activity mActivity;
    private RecentActivityListCoordinator mCoordinator;

    @Before
    public void setup() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        doNothing().when(mBottomSheetController).addObserver(mBottomSheetObserverCaptor.capture());
        mCoordinator =
                new RecentActivityListCoordinator(
                        mActivity,
                        mBottomSheetController,
                        mMessagingBackendService,
                        mFaviconProvider,
                        mAvatarProvider,
                        mRecentActivityActionHandler);
        verify(mBottomSheetController).addObserver(any());
    }

    @Test
    public void testOpenBottomSheet() {
        mCoordinator.requestShowUI(TEST_COLLABORATION_ID1);
        verify(mBottomSheetController).requestShowContent(any(), eq(true));
    }

    @Test
    public void testCloseBottomSheetRemovesBottomSheetObserver() {
        mCoordinator.requestShowUI(TEST_COLLABORATION_ID1);
        verify(mBottomSheetController).requestShowContent(any(), eq(true));
        mBottomSheetObserverCaptor.getValue().onSheetClosed(0);
        verify(mBottomSheetController).removeObserver(any());
    }
}
