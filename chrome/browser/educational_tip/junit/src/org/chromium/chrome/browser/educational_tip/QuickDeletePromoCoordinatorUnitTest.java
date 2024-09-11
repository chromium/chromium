// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import static org.mockito.Mockito.verify;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.CallbackController;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.educational_tip.cards.QuickDeletePromoCoordinator;
import org.chromium.ui.shadows.ShadowAppCompatResources;

/** Test relating to {@link QuickDeletePromoCoordinator} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowAppCompatResources.class})
public class QuickDeletePromoCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Runnable mOnModuleClickedCallback;
    @Mock private EducationTipModuleActionDelegate mActionDelegate;

    private QuickDeletePromoCoordinator mQuickDeletePromoCoordinator;

    @Before
    public void setUp() {
        mQuickDeletePromoCoordinator =
                new QuickDeletePromoCoordinator(
                        mOnModuleClickedCallback, new CallbackController(), mActionDelegate);
    }

    @Test
    @SmallTest
    public void testClickTabGroupPromoCard() {
        mQuickDeletePromoCoordinator.onCardClicked();
        verify(mActionDelegate).openAndHighlightQuickDeleteMenuItem();
        verify(mOnModuleClickedCallback).run();
    }
}
