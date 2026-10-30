// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_promo.bottom_sheet;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.view.View;
import android.widget.TextView;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.safety_promo.R;
import org.chromium.chrome.browser.safety_promo.SafetyPromoItem;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/** Unit tests for {@link SafetyPromoBottomSheetCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SafetyPromoBottomSheetCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private Runnable mOnDestroyRunnable;
    @Captor private ArgumentCaptor<BottomSheetContent> mBottomSheetContentCaptor;

    private SafetyPromoBottomSheetCoordinator mCoordinator;

    @Before
    public void setUp() {
        Context context = ApplicationProvider.getApplicationContext();
        context.setTheme(R.style.Theme_BrowserUI_DayNight);
        mCoordinator =
                new SafetyPromoBottomSheetCoordinator(
                        context,
                        mBottomSheetController,
                        SafetyPromoItem.INCOGNITO,
                        mOnDestroyRunnable);
    }

    @Test
    public void testTitle_isItemBottomSheetTitle() {
        View contentView = showBottomSheetAndGetContent().getContentView();

        TextView titleView = contentView.findViewById(R.id.safety_promo_bottom_sheet_title);
        assertEquals(
                contentView.getContext().getString(SafetyPromoItem.INCOGNITO.bottomSheetTitleResId),
                titleView.getText().toString());
    }

    @Test
    public void testShowBottomSheet() {
        mCoordinator.showBottomSheet();

        verify(mBottomSheetController)
                .requestShowContent(mBottomSheetContentCaptor.capture(), /* animate= */ eq(true));
    }

    @Test
    public void testDismissBottomSheet() {
        BottomSheetContent content = showBottomSheetAndGetContent();

        mCoordinator.dismissBottomSheet(/* animate= */ false);

        verify(mBottomSheetController).hideContent(eq(content), /* animate= */ eq(false));
    }

    @Test
    public void testGotItClick_dismissesBottomSheet() {
        BottomSheetContent content = showBottomSheetAndGetContent();

        content.getContentView()
                .findViewById(R.id.safety_promo_bottom_sheet_got_it_button)
                .performClick();

        verify(mBottomSheetController).hideContent(eq(content), /* animate= */ eq(true));
    }

    @Test
    public void testContentDestroy_runsOnDestroyAndClearsGotItListener() {
        BottomSheetContent content = showBottomSheetAndGetContent();

        content.destroy();

        verify(mOnDestroyRunnable).run();
        content.getContentView()
                .findViewById(R.id.safety_promo_bottom_sheet_got_it_button)
                .performClick();
        verify(mBottomSheetController, never()).hideContent(any(), anyBoolean());
    }

    private BottomSheetContent showBottomSheetAndGetContent() {
        mCoordinator.showBottomSheet();
        verify(mBottomSheetController)
                .requestShowContent(mBottomSheetContentCaptor.capture(), /* animate= */ eq(true));
        return mBottomSheetContentCaptor.getValue();
    }
}
