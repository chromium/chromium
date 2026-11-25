// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.CallbackController;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.educational_tip.cards.FuseboxPromoCoordinator;

/** Test relating to {@link FuseboxPromoCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FuseboxPromoCoordinatorUnitTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private Runnable mOnModuleClickedCallback;
    @Mock private EducationTipModuleActionDelegate mActionDelegate;

    private FuseboxPromoCoordinator mFuseboxPromoCoordinator;

    @Before
    public void setUp() {
        mFuseboxPromoCoordinator =
                new FuseboxPromoCoordinator(
                        mOnModuleClickedCallback, new CallbackController(), mActionDelegate);
    }

    @Test
    public void testClickFuseboxPromoCard() {
        mFuseboxPromoCoordinator.onCardClicked();

        verify(mActionDelegate).startFuseboxGuidedTour();
        verify(mOnModuleClickedCallback).run();
    }
}
