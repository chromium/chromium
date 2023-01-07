// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tasks.tab_management.MessageService.MessageType;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/**
 * Unit tests for {@link IphMessageService}.
 */
@SuppressWarnings({"ResultOfMethodCallIgnored", "ArraysAsListWithZeroOrOneArgument"})
@RunWith(LocalRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
public class IphMessageServiceUnitTest {
    @Mock
    private TabSwitcherCoordinator.IphController mIphController;

    @Mock
    private Profile mProfile;

    @Mock
    private Tracker mTracker;

    @Mock
    private MessageService.MessageObserver mMessageObserver;

    private IphMessageService mIphMessageService;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        Profile.setLastUsedProfileForTesting(mProfile);
        TrackerFactory.setTrackerForTests(mTracker);
        mIphMessageService = new IphMessageService(mIphController);
    }

    @After
    public void tearDown() {
        Profile.setLastUsedProfileForTesting(null);
        TrackerFactory.setTrackerForTests(null);
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
    public void testAddObserver() {
        mIphMessageService.addObserver(mMessageObserver);
        Assert.assertTrue(
                mIphMessageService.getObserversForTesting().hasObserver(mMessageObserver));
        verify(mTracker, times(1))
                .addOnInitializedCallback(mIphMessageService.getInitializedCallbackForTesting());
    }

    @Test
    public void testCallbackWouldTriggerDragDrop() {
        doReturn(true).when(mTracker).wouldTriggerHelpUI(
                eq(FeatureConstants.TAB_GROUPS_DRAG_AND_DROP_FEATURE));
        doReturn(true).when(mTracker).isInitialized();
        mIphMessageService.addObserver(mMessageObserver);
        mIphMessageService.getInitializedCallbackForTesting().onResult(true);
        verify(mMessageObserver, times(1))
                .messageReady(eq(MessageType.IPH), any(IphMessageService.IphMessageData.class));
    }

    @Test
    public void testCallbackWouldNotTriggerDragDrop() {
        doReturn(false).when(mTracker).wouldTriggerHelpUI(
                eq(FeatureConstants.TAB_GROUPS_DRAG_AND_DROP_FEATURE));
        mIphMessageService.addObserver(mMessageObserver);
        mIphMessageService.getInitializedCallbackForTesting().onResult(true);
        verify(mMessageObserver, times(0))
                .messageReady(eq(MessageType.IPH), any(IphMessageService.IphMessageData.class));
    }
}
