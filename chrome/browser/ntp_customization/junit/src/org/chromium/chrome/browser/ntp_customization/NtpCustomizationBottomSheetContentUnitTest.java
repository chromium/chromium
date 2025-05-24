// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.FEED;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.MAIN;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.NTP_CARDS;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.MathUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/** Unit tests for {@link NtpCustomizationBottomSheetContent}. */
@RunWith(BaseRobolectricTestRunner.class)
public final class NtpCustomizationBottomSheetContentUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Runnable mBackPressRunnable;
    @Mock private Runnable mOnDestroyRunnable;
    @Mock private ObservableSupplierImpl<Boolean> mObservableSupplier;

    private Context mContext;
    private NtpCustomizationBottomSheetContent mBottomSheetContent;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        View view =
                LayoutInflater.from(mContext)
                        .inflate(R.layout.ntp_customization_bottom_sheet, /* root= */ null);
        mBottomSheetContent =
                new NtpCustomizationBottomSheetContent(
                        view, mBackPressRunnable, mOnDestroyRunnable, mock(Supplier.class));
    }

    @Test
    public void testBasics() {
        assertNotNull(mBottomSheetContent.getContentView());
        assertNull(mBottomSheetContent.getToolbarView());
        assertEquals(0, mBottomSheetContent.getVerticalScrollOffset());
        assertEquals(BottomSheetContent.ContentPriority.HIGH, mBottomSheetContent.getPriority());
        assertFalse(mBottomSheetContent.swipeToDismissEnabled());
        assertEquals(BottomSheetContent.HeightMode.DISABLED, mBottomSheetContent.getPeekHeight());
        assertEquals(
                (float) BottomSheetContent.HeightMode.WRAP_CONTENT,
                mBottomSheetContent.getFullHeightRatio(),
                MathUtils.EPSILON);
        assertEquals(
                R.string.ntp_customization_main_bottom_sheet_closed,
                mBottomSheetContent.getSheetClosedAccessibilityStringId());
    }

    @Test
    public void testAccessibilityStrings() {
        // Verifies the expected content description and accessibility string when the main bottom
        // sheet is fully expanded and when it's closed.
        mBottomSheetContent.setCurrentBottomSheetTypeSupplierForTesting(() -> MAIN);
        assertNull(mBottomSheetContent.getSheetContentDescription(mContext));
        assertEquals(
                R.string.ntp_customization_main_bottom_sheet_opened_full,
                mBottomSheetContent.getSheetFullHeightAccessibilityStringId());
        assertEquals(
                R.string.ntp_customization_main_bottom_sheet_closed,
                mBottomSheetContent.getSheetClosedAccessibilityStringId());

        // Verifies the expected content description and accessibility string when the NTP cards
        // bottom sheet is fully expanded.
        mBottomSheetContent.setCurrentBottomSheetTypeSupplierForTesting(() -> NTP_CARDS);
        assertEquals(
                "New tab page cards bottom sheet",
                mBottomSheetContent.getSheetContentDescription(mContext));
        assertEquals(
                R.string.ntp_customization_ntp_cards_bottom_sheet_opened_full,
                mBottomSheetContent.getSheetFullHeightAccessibilityStringId());

        // Verifies the expected content description and accessibility string when the feed bottom
        // sheet is fully expanded.
        mBottomSheetContent.setCurrentBottomSheetTypeSupplierForTesting(() -> FEED);
        assertEquals(
                "Discover feed bottom sheet",
                mBottomSheetContent.getSheetContentDescription(mContext));
        assertEquals(
                R.string.ntp_customization_feed_bottom_sheet_opened_full,
                mBottomSheetContent.getSheetFullHeightAccessibilityStringId());
    }

    @Test
    public void testHandleBackPress() {
        mBottomSheetContent.onBackPressed();
        verify(mBackPressRunnable).run();
        clearInvocations(mBackPressRunnable);

        assertTrue(mBottomSheetContent.handleBackPress());
        verify(mBackPressRunnable).run();
    }

    @Test
    public void testSheetClosedAndOpened() {
        mBottomSheetContent.setBackPressStateChangedSupplierForTesting(mObservableSupplier);
        mBottomSheetContent.onSheetOpened();
        verify(mObservableSupplier).set(eq(true));

        mBottomSheetContent.onSheetClosed();
        verify(mObservableSupplier).set(eq(false));
    }

    @Test
    public void testDestroy() {
        mBottomSheetContent.destroy();
        verify(mOnDestroyRunnable).run();
    }
}
