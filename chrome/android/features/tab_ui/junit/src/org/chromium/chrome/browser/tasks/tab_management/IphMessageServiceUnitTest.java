// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab_ui.TabSwitcherIphController;
import org.chromium.chrome.browser.tasks.tab_management.MessageCardView.ServiceDismissActionProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.MessageType;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;

/** Unit tests for {@link IphMessageService}. */
@SuppressWarnings({"ResultOfMethodCallIgnored", "ArraysAsListWithZeroOrOneArgument"})
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
public class IphMessageServiceUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Context mContext;

    @Mock private TabSwitcherIphController mIphController;

    @Mock private Profile mProfile;

    @Mock private Tracker mTracker;

    @Mock private ServiceDismissActionProvider<@MessageType Integer> mServiceDismissActionProvider;

    private IphMessageService mIphMessageService;

    @Before
    public void setUp() {
        IphMessageService.setSkipIphInTestsForTesting(false);
        TrackerFactory.setTrackerForTests(mTracker);
        mIphMessageService = new IphMessageService(mContext, () -> mProfile, mIphController);
    }

    @Test
    public void testReview() {
        mIphMessageService.review();
        verify(mIphController, times(1)).showIph();
    }

    @Test
    public void testDismiss() {
        mIphMessageService.initialize(mServiceDismissActionProvider);
        mIphMessageService.dismiss();
        verify(mTracker, times(1))
                .shouldTriggerHelpUi(FeatureConstants.TAB_GROUPS_DRAG_AND_DROP_FEATURE);
        verify(mTracker, times(1)).dismissed(FeatureConstants.TAB_GROUPS_DRAG_AND_DROP_FEATURE);
    }

    @Test
    public void testInitialize_TrackerNotInitialized() {
        doReturn(false).when(mTracker).isInitialized();
        mIphMessageService.initialize(mServiceDismissActionProvider);
        verify(mTracker, times(1))
                .addOnInitializedCallback(mIphMessageService.getInitializedCallbackForTesting());
    }

    @Test
    public void testInitialize_TrackerInitialized() {
        doReturn(true)
                .when(mTracker)
                .wouldTriggerHelpUi(eq(FeatureConstants.TAB_GROUPS_DRAG_AND_DROP_FEATURE));
        doReturn(true).when(mTracker).isInitialized();
        mIphMessageService.initialize(mServiceDismissActionProvider);
        assertFalse(mIphMessageService.getMessageItems().isEmpty());
    }

    @Test
    public void testCallbackWouldTriggerDragDrop() {
        doReturn(true)
                .when(mTracker)
                .wouldTriggerHelpUi(eq(FeatureConstants.TAB_GROUPS_DRAG_AND_DROP_FEATURE));
        doReturn(false).when(mTracker).isInitialized();
        mIphMessageService.initialize(mServiceDismissActionProvider);
        doReturn(true).when(mTracker).isInitialized();
        mIphMessageService.getInitializedCallbackForTesting().onResult(true);
        assertFalse(mIphMessageService.getMessageItems().isEmpty());
    }

    @Test
    public void testCallbackWouldNotTriggerDragDrop() {
        doReturn(false)
                .when(mTracker)
                .wouldTriggerHelpUi(eq(FeatureConstants.TAB_GROUPS_DRAG_AND_DROP_FEATURE));
        mIphMessageService.initialize(mServiceDismissActionProvider);
        mIphMessageService.getInitializedCallbackForTesting().onResult(true);
        assertTrue(mIphMessageService.getMessageItems().isEmpty());
    }
}
