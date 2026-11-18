// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import static org.mockito.Mockito.inOrder;

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

    @Test
    public void testClickTipsNotificationsPromoCard() {
        mCoordinator.onCardClicked();
        InOrder inOrder = inOrder(mActionDelegate, mOnModuleClickedCallback, mRemoveModuleCallback);
        inOrder.verify(mActionDelegate).optInToTipsNotifications();
        inOrder.verify(mOnModuleClickedCallback).run();
        inOrder.verify(mRemoveModuleCallback).run();
    }
}
