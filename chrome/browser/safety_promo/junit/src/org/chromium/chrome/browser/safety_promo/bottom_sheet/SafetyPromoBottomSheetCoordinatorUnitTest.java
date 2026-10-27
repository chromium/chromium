// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_promo.bottom_sheet;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.safety_promo.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/** Unit tests for {@link SafetyPromoBottomSheetCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SafetyPromoBottomSheetCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private SafetyPromoBottomSheetContent mBottomSheetContent;
    @Mock private Runnable mOnDestroyRunnable;

    private SafetyPromoBottomSheetCoordinator mCoordinator;

    @Before
    public void setUp() {
        Context context = ApplicationProvider.getApplicationContext();
        context.setTheme(R.style.Theme_BrowserUI_DayNight);
        mCoordinator =
                new SafetyPromoBottomSheetCoordinator(
                        context, mBottomSheetController, mOnDestroyRunnable);
        mCoordinator.setBottomSheetContentForTesting(mBottomSheetContent);
    }

    @Test
    public void testShowBottomSheet() {
        mCoordinator.showBottomSheet();

        verify(mBottomSheetController)
                .requestShowContent(eq(mBottomSheetContent), /* animate= */ eq(true));
    }

    @Test
    public void testDismissBottomSheet() {
        mCoordinator.dismissBottomSheet(/* animate= */ false);

        verify(mBottomSheetController)
                .hideContent(eq(mBottomSheetContent), /* animate= */ eq(false));
    }
}
