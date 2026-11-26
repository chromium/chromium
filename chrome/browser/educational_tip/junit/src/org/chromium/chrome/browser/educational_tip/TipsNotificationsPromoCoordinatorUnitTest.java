// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.CallbackController;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.educational_tip.cards.TipsNotificationsPromoCoordinator;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/** Test relating to {@link TipsNotificationsPromoCoordinator} */
@RunWith(BaseRobolectricTestRunner.class)
public class TipsNotificationsPromoCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Runnable mOnModuleClickedCallback;
    @Mock private Runnable mRemoveModuleCallback;
    @Mock private EducationTipModuleActionDelegate mActionDelegate;

    private TipsNotificationsPromoCoordinator mCoordinator;

    @Before
    public void setUp() {
        mCoordinator =
                new TipsNotificationsPromoCoordinator(
                        mOnModuleClickedCallback,
                        new CallbackController(),
                        mActionDelegate,
                        mRemoveModuleCallback);
    }

    @After
    public void tearDown() {
        mCoordinator.destroy();
    }

    @Test
    public void testClickTipsNotificationsPromoCard() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.TIPS_NOTIFICATIONS_OPT_IN_ELIGIBLE, true);
        doAnswer(
                        invocation -> {
                            ChromeSharedPreferences.getInstance()
                                    .writeBoolean(
                                            ChromePreferenceKeys.TIPS_NOTIFICATIONS_OPT_IN_ELIGIBLE,
                                            false);
                            return null;
                        })
                .when(mActionDelegate)
                .optInToTipsNotifications();

        mCoordinator.onCardClicked();

        InOrder inOrder = inOrder(mActionDelegate, mOnModuleClickedCallback, mRemoveModuleCallback);
        inOrder.verify(mActionDelegate).optInToTipsNotifications();
        inOrder.verify(mOnModuleClickedCallback).run();
        inOrder.verify(mRemoveModuleCallback).run();
        // Verify the observer was unregistered before opting in and did not double-remove.
        verify(mRemoveModuleCallback).run();
    }

    @Test
    public void testEligibilityPrefBecomesFalse_RemovesModule() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.TIPS_NOTIFICATIONS_OPT_IN_ELIGIBLE, true);
        verify(mRemoveModuleCallback, never()).run();

        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.TIPS_NOTIFICATIONS_OPT_IN_ELIGIBLE, false);
        verify(mRemoveModuleCallback).run();
    }

    @Test
    public void testDestroy_StopsObservingEligibilityPref() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.TIPS_NOTIFICATIONS_OPT_IN_ELIGIBLE, true);
        mCoordinator.destroy();

        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.TIPS_NOTIFICATIONS_OPT_IN_ELIGIBLE, false);
        verify(mRemoveModuleCallback, never()).run();
    }
}
