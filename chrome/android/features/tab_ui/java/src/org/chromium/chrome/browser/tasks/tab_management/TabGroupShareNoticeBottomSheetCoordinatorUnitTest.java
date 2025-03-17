// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.ContextThemeWrapper;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupShareNoticeBottomSheetCoordinator.TabGroupShareNoticeBottomSheetCoordinatorDelegate;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;

/** Unit tests for {@link TabGroupShareNoticeBottomSheetCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabGroupShareNoticeBottomSheetCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private Profile mProfile;
    @Mock private Tracker mTracker;
    private TabGroupShareNoticeBottomSheetCoordinator mCoordinator;

    @Before
    public void setUp() {
        TrackerFactory.setTrackerForTests(mTracker);
        Context mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        mCoordinator =
                new TabGroupShareNoticeBottomSheetCoordinator(
                        mBottomSheetController, mContext, mProfile);
        when(mTracker.shouldTriggerHelpUi(FeatureConstants.TAB_GROUP_SHARE_NOTICE_FEATURE))
                .thenReturn(true);
        when(mBottomSheetController.requestShowContent(any(), eq(true))).thenReturn(true);
    }

    @Test
    public void testRequestShowContent() {
        mCoordinator.requestShowContent();
        verify(mBottomSheetController)
                .requestShowContent(any(TabGroupShareNoticeBottomSheetView.class), eq(true));
        verify(mTracker).shouldTriggerHelpUi(FeatureConstants.TAB_GROUP_SHARE_NOTICE_FEATURE);
    }

    @Test
    public void testHideContent() {
        mCoordinator.requestShowContent();
        verify(mBottomSheetController).requestShowContent(any(), anyBoolean());

        TabGroupShareNoticeBottomSheetCoordinatorDelegate delegate = mCoordinator.initDelegate();
        delegate.hide(StateChangeReason.INTERACTION_COMPLETE);

        verify(mBottomSheetController)
                .hideContent(
                        any(TabGroupShareNoticeBottomSheetView.class),
                        eq(true),
                        eq(StateChangeReason.INTERACTION_COMPLETE));
    }

    @Test
    public void testRequestShowContent_BottomSheetControllerFails() {
        when(mBottomSheetController.requestShowContent(any(), eq(true))).thenReturn(false);
        mCoordinator.requestShowContent();
        verify(mBottomSheetController, never()).addObserver(any());
    }

    @Test
    public void testRequestShowContent_TrackerReturnsFalse() {
        when(mTracker.shouldTriggerHelpUi(FeatureConstants.TAB_GROUP_SHARE_NOTICE_FEATURE))
                .thenReturn(false);
        mCoordinator.requestShowContent();
        verify(mBottomSheetController, never()).addObserver(any());
    }

    @Test
    public void testMarkHasReadNotice() {
        TabGroupShareNoticeBottomSheetCoordinatorDelegate delegate = mCoordinator.initDelegate();
        delegate.onSheetClosed();

        verify(mTracker).notifyEvent("tab_group_share_notice_dismissed");
        verify(mTracker).dismissed(FeatureConstants.TAB_GROUP_SHARE_NOTICE_FEATURE);
    }
}
