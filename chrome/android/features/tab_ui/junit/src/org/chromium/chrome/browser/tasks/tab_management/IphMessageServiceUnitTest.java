// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab_ui.TabSwitcherIphController;
import org.chromium.chrome.browser.tasks.tab_management.MessageService.MessageType;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;

/** Unit tests for {@link IphMessageService}. */
@SuppressWarnings({"ResultOfMethodCallIgnored", "ArraysAsListWithZeroOrOneArgument"})
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
public class IphMessageServiceUnitTest {
    @Mock private TabSwitcherIphController mIphController;

    @Mock private Profile mProfile;

    @Mock private Tracker mTracker;

    @Mock private MessageService.MessageObserver mMessageObserver;

    private IphMessageService mIphMessageService;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        IphMessageService.setSkipIphInTestsForTesting(false);
        TrackerFactory.setTrackerForTests(mTracker);
        mIphMessageService = new IphMessageService(mProfile, mIphController);
    }

    @Test
    public void testReview() {
        mIphMessageService.review();
        verify(mIphController, times(1)).showIph();
    }

    @Test
    public void testDismiss() {
        mIphMessageService.dismiss();
        verify(mTracker, times(1))
                .shouldTriggerHelpUI(FeatureConstants.TAB_GROUPS_DRAG_AND_DROP_FEATURE);
        verify(mTracker, times(1)).dismissed(FeatureConstants.TAB_GROUPS_DRAG_AND_DROP_FEATURE);
    }

    @Test
    public void testAddObserver_NotInitialized() {
        doReturn(false).when(mTracker).isInitialized();
        mIphMessageService.addObserver(mMessageObserver);
        Assert.assertTrue(
                mIphMessageService.getObserversForTesting().hasObserver(mMessageObserver));
        verify(mTracker, times(1))
                .addOnInitializedCallback(mIphMessageService.getInitializedCallbackForTesting());
    }

    @Test
    public void testAddObserver_Initialized() {
        doReturn(true)
                .when(mTracker)
                .wouldTriggerHelpUI(eq(FeatureConstants.TAB_GROUPS_DRAG_AND_DROP_FEATURE));
        doReturn(true).when(mTracker).isInitialized();
        mIphMessageService.addObserver(mMessageObserver);
        Assert.assertTrue(
                mIphMessageService.getObserversForTesting().hasObserver(mMessageObserver));
        verify(mMessageObserver, times(1))
                .messageReady(eq(MessageType.IPH), any(IphMessageService.IphMessageData.class));
    }

    @Test
    public void testCallbackWouldTriggerDragDrop() {
        doReturn(true)
                .when(mTracker)
                .wouldTriggerHelpUI(eq(FeatureConstants.TAB_GROUPS_DRAG_AND_DROP_FEATURE));
        doReturn(false).when(mTracker).isInitialized();
        mIphMessageService.addObserver(mMessageObserver);
        doReturn(true).when(mTracker).isInitialized();
        mIphMessageService.getInitializedCallbackForTesting().onResult(true);
        verify(mMessageObserver, times(1))
                .messageReady(eq(MessageType.IPH), any(IphMessageService.IphMessageData.class));
    }

    @Test
    public void testCallbackWouldNotTriggerDragDrop() {
        doReturn(false)
                .when(mTracker)
                .wouldTriggerHelpUI(eq(FeatureConstants.TAB_GROUPS_DRAG_AND_DROP_FEATURE));
        mIphMessageService.addObserver(mMessageObserver);
        mIphMessageService.getInitializedCallbackForTesting().onResult(true);
        verify(mMessageObserver, times(0))
                .messageReady(eq(MessageType.IPH), any(IphMessageService.IphMessageData.class));
    }
}
