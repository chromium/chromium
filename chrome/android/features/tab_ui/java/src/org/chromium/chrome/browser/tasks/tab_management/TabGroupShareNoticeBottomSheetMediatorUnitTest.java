// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupShareNoticeBottomSheetCoordinator.TabGroupShareNoticeBottomSheetCoordinatorDelegate;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;

/** Unit tests for {@link TabGroupShareNoticeBottomSheetMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabGroupShareNoticeBottomSheetMediatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private TabGroupShareNoticeBottomSheetCoordinatorDelegate mDelegate;
    @Mock private Tracker mTracker;
    private TabGroupShareNoticeBottomSheetMediator mMediator;

    @Before
    public void setUp() {
        mMediator =
                new TabGroupShareNoticeBottomSheetMediator(
                        mBottomSheetController, mDelegate, mTracker);
        when(mTracker.shouldTriggerHelpUi(FeatureConstants.TAB_GROUP_SHARE_NOTICE_FEATURE))
                .thenReturn(true);
        when(mDelegate.requestShowContent()).thenReturn(true);
    }

    @Test
    public void testDisplayNotice() {
        mMediator.requestShowContent();
        verify(mDelegate).requestShowContent();
    }

    @Test
    public void testShouldNotDisplayNotice() {
        when(mTracker.shouldTriggerHelpUi(FeatureConstants.TAB_GROUP_SHARE_NOTICE_FEATURE))
                .thenReturn(false);
        mMediator.requestShowContent();
        verify(mDelegate, never()).requestShowContent();
    }

    @Test
    public void testHide() {
        mMediator.hide(BottomSheetController.StateChangeReason.SWIPE);
        verify(mDelegate).hide(BottomSheetController.StateChangeReason.SWIPE);
    }

    @Test
    public void testMarkHasReadNotice() {
        mMediator.markHasReadNotice();
        verify(mTracker).notifyEvent("tab_group_share_notice_dismissed");
        verify(mTracker).dismissed(FeatureConstants.TAB_GROUP_SHARE_NOTICE_FEATURE);
    }
}
