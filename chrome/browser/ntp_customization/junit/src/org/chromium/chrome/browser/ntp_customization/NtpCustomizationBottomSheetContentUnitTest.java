// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.CHROME_COLORS;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.FEED;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.MAIN;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.NTP_CARDS;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.SINGLE_THEME_COLLECTION;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.THEME;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.THEME_COLLECTIONS;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

import java.util.function.Supplier;

/** Unit tests for {@link NtpCustomizationBottomSheetContent}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowLooper.class})
public final class NtpCustomizationBottomSheetContentUnitTest {
    private static final float FLOATING_POINT_DELTA = 0.1f;
    private static final int CONTAINER_HEIGHT = 2000;
    private static final int MAX_SHEET_WIDTH = 1000;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Runnable mBackPressRunnable;
    @Mock private Runnable mOnDestroyRunnable;
    @Mock private ObservableSupplierImpl<Boolean> mObservableSupplier;
    @Mock private Supplier<Integer> mContainerHeightSupplier;
    @Mock private Supplier<Integer> mMaxSheetWidthSupplier;
    @Mock private RecyclerView mThemeCollectionsRecyclerView;
    @Mock private RecyclerView mSingleThemeCollectionRecyclerView;
    @Mock private RecyclerView mChromeColorsRecyclerView;
    @Mock private View mChromeColorsRecyclerViewContainer;
    @Mock private View mViewFlipper;

    private Context mContext;
    private View mView;
    private NtpCustomizationBottomSheetContent mBottomSheetContent;
    private Supplier<Integer> mBottomSheetTypeSupplier;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mView =
                spy(
                        LayoutInflater.from(mContext)
                                .inflate(
                                        R.layout.ntp_customization_bottom_sheet, /* root= */ null));

        when(mView.findViewById(R.id.theme_collections_recycler_view))
                .thenReturn(mThemeCollectionsRecyclerView);
        when(mView.findViewById(R.id.single_theme_collection_recycler_view))
                .thenReturn(mSingleThemeCollectionRecyclerView);
        when(mView.findViewById(R.id.chrome_colors_recycler_view))
                .thenReturn(mChromeColorsRecyclerView);
        when(mView.findViewById(R.id.chrome_colors_recycler_view_container))
                .thenReturn(mChromeColorsRecyclerViewContainer);
        when(mView.findViewById(R.id.ntp_customization_view_flipper)).thenReturn(mViewFlipper);
        when(mContainerHeightSupplier.get()).thenReturn(CONTAINER_HEIGHT);
        when(mMaxSheetWidthSupplier.get()).thenReturn(MAX_SHEET_WIDTH);

        mBottomSheetTypeSupplier = () -> MAIN;
        mBottomSheetContent =
                new NtpCustomizationBottomSheetContent(
                        mView,
                        mContainerHeightSupplier,
                        mMaxSheetWidthSupplier,
                        mBackPressRunnable,
                        mOnDestroyRunnable,
                        () -> mBottomSheetTypeSupplier.get());
    }

    @Test
    public void testBasics() {
        assertNotNull(mBottomSheetContent.getContentView());
        assertNull(mBottomSheetContent.getToolbarView());
        assertEquals(BottomSheetContent.ContentPriority.HIGH, mBottomSheetContent.getPriority());
        assertFalse(mBottomSheetContent.swipeToDismissEnabled());
        assertEquals(
                BottomSheetContent.HeightMode.DISABLED,
                mBottomSheetContent.getHalfHeightRatio(),
                FLOATING_POINT_DELTA);
        assertEquals(
                R.string.ntp_customization_main_bottom_sheet_closed,
                mBottomSheetContent.getSheetClosedAccessibilityStringId());
    }

    @Test
    public void testFullHeightRatio_noActiveRecyclerView() {
        mBottomSheetTypeSupplier = () -> MAIN;
        assertNull(mBottomSheetContent.getActiveRecyclerView());
        assertEquals(
                BottomSheetContent.HeightMode.WRAP_CONTENT,
                mBottomSheetContent.getFullHeightRatio(),
                FLOATING_POINT_DELTA);
    }

    @Test
    public void testHeightRatiosAndPadding_withActiveRecyclerView() {
        mBottomSheetTypeSupplier = () -> THEME_COLLECTIONS;
        assertEquals(mThemeCollectionsRecyclerView, mBottomSheetContent.getActiveRecyclerView());

        // Case 1: recyclerView.getBottom() is 0, getContentHeight returns
        // RECYCLER_VIEW_INVALID_HEIGHT.
        when(mThemeCollectionsRecyclerView.getBottom()).thenReturn(0);
        assertEquals(
                BottomSheetContent.HeightMode.DISABLED,
                mBottomSheetContent.getHalfHeightRatio(),
                FLOATING_POINT_DELTA);
        assertEquals(
                BottomSheetContent.HeightMode.WRAP_CONTENT,
                mBottomSheetContent.getFullHeightRatio(),
                FLOATING_POINT_DELTA);
        // setPaddingRelative should not be called.
        verify(mThemeCollectionsRecyclerView, never())
                .setPaddingRelative(anyInt(), anyInt(), anyInt(), anyInt());

        // Reset for next cases.
        when(mThemeCollectionsRecyclerView.getPaddingStart()).thenReturn(0);
        when(mThemeCollectionsRecyclerView.getPaddingTop()).thenReturn(0);
        when(mThemeCollectionsRecyclerView.getPaddingEnd()).thenReturn(0);

        // Case 2: Content height is small (<= 0.5 * container height), no overflow.
        when(mView.getMeasuredHeight()).thenReturn((int) (0.4 * CONTAINER_HEIGHT));
        when(mThemeCollectionsRecyclerView.getBottom())
                .thenReturn(
                        (int)
                                (0.8
                                        * NtpCustomizationBottomSheetContent.MAX_HEIGHT_RATIO
                                        * CONTAINER_HEIGHT));
        assertEquals(
                BottomSheetContent.HeightMode.DISABLED,
                mBottomSheetContent.getHalfHeightRatio(),
                FLOATING_POINT_DELTA);
        assertEquals(
                BottomSheetContent.HeightMode.WRAP_CONTENT,
                mBottomSheetContent.getFullHeightRatio(),
                FLOATING_POINT_DELTA);
        verify(mThemeCollectionsRecyclerView, times(2)).setPaddingRelative(0, 0, 0, 0);

        // Case 3: Content height is medium (> 0.5, <= MAX_HEIGHT_RATIO), no overflow.
        when(mView.getMeasuredHeight()).thenReturn((int) (0.6 * CONTAINER_HEIGHT));
        assertEquals(0.5f, mBottomSheetContent.getHalfHeightRatio(), FLOATING_POINT_DELTA);
        assertEquals(0.6f, mBottomSheetContent.getFullHeightRatio(), FLOATING_POINT_DELTA);
        verify(mThemeCollectionsRecyclerView, times(4)).setPaddingRelative(0, 0, 0, 0);

        // Case 4: Content overflows. Check that padding is set.
        float overflow = 100f;
        float maxHeight = NtpCustomizationBottomSheetContent.MAX_HEIGHT_RATIO * CONTAINER_HEIGHT;
        when(mThemeCollectionsRecyclerView.getBottom()).thenReturn((int) (maxHeight + overflow));
        assertEquals(0.5f, mBottomSheetContent.getHalfHeightRatio(), FLOATING_POINT_DELTA);
        assertEquals(
                (float) NtpCustomizationBottomSheetContent.MAX_HEIGHT_RATIO,
                mBottomSheetContent.getFullHeightRatio(),
                FLOATING_POINT_DELTA);
        verify(mThemeCollectionsRecyclerView, times(2))
                .setPaddingRelative(0, 0, 0, (int) Math.ceil(overflow));
    }

    @Test
    public void testGetActiveRecyclerView() {
        // Returns the theme collections recycler view for THEME_COLLECTIONS type.
        mBottomSheetTypeSupplier = () -> THEME_COLLECTIONS;
        assertEquals(mThemeCollectionsRecyclerView, mBottomSheetContent.getActiveRecyclerView());

        // Returns the single theme collection recycler view for SINGLE_THEME_COLLECTION type.
        mBottomSheetTypeSupplier = () -> SINGLE_THEME_COLLECTION;
        assertEquals(
                mSingleThemeCollectionRecyclerView, mBottomSheetContent.getActiveRecyclerView());

        // Returns the chrome colors recycler view for CHROME_COLORS type.
        mBottomSheetTypeSupplier = () -> CHROME_COLORS;
        assertEquals(mChromeColorsRecyclerView, mBottomSheetContent.getActiveRecyclerView());

        // Returns null for other types like MAIN.
        mBottomSheetTypeSupplier = () -> MAIN;
        assertNull(mBottomSheetContent.getActiveRecyclerView());
    }

    @Test
    public void testGetVerticalScrollOffset() {
        int expectScrollOffset = 10;

        // With no active RecyclerView, returns the ViewFlipper's scroll Y.
        mBottomSheetTypeSupplier = () -> MAIN;
        when(mViewFlipper.getScrollY()).thenReturn(expectScrollOffset);
        assertEquals(expectScrollOffset, mBottomSheetContent.getVerticalScrollOffset());

        // With an active RecyclerView, returns its vertical scroll offset.
        expectScrollOffset = 20;
        mBottomSheetTypeSupplier = () -> THEME_COLLECTIONS;
        when(mThemeCollectionsRecyclerView.computeVerticalScrollOffset())
                .thenReturn(expectScrollOffset);
        assertEquals(expectScrollOffset, mBottomSheetContent.getVerticalScrollOffset());
    }

    @Test
    public void testAccessibilityStrings() {
        // Verifies the expected content description and accessibility string when the main bottom
        // sheet is fully expanded and when it's closed.
        mBottomSheetContent.setCurrentBottomSheetTypeSupplierForTesting(() -> MAIN);
        assertEquals(
                "Customize your new tab page bottom sheet",
                mBottomSheetContent.getSheetContentDescription(mContext));
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

        // Verifies the expected content description and accessibility string when the theme bottom
        // sheet is fully expanded.
        mBottomSheetContent.setCurrentBottomSheetTypeSupplierForTesting(() -> THEME);
        assertEquals(
                "New tab page appearance bottom sheet",
                mBottomSheetContent.getSheetContentDescription(mContext));
        assertEquals(
                R.string.ntp_customization_theme_bottom_sheet_opened_full,
                mBottomSheetContent.getSheetFullHeightAccessibilityStringId());
        assertEquals(
                R.string.ntp_customization_theme_bottom_sheet_opened_half,
                mBottomSheetContent.getSheetHalfHeightAccessibilityStringId());

        // Verifies the expected content description and accessibility string when the theme
        // collections bottom sheet is fully expanded.
        mBottomSheetContent.setCurrentBottomSheetTypeSupplierForTesting(() -> THEME_COLLECTIONS);
        assertEquals(
                "New tab page appearance theme collections bottom sheet",
                mBottomSheetContent.getSheetContentDescription(mContext));
        Assert.assertEquals(
                R.string.ntp_customization_theme_collections_bottom_sheet_opened_full,
                mBottomSheetContent.getSheetFullHeightAccessibilityStringId());

        // Verifies the expected content description and accessibility string when a single
        // theme collection bottom sheet is fully expanded.
        mBottomSheetContent.setCurrentBottomSheetTypeSupplierForTesting(
                () -> SINGLE_THEME_COLLECTION);
        assertEquals(
                "New tab page appearance theme collections bottom sheet",
                mBottomSheetContent.getSheetContentDescription(mContext));
        Assert.assertEquals(
                R.string.ntp_customization_theme_collections_bottom_sheet_opened_full,
                mBottomSheetContent.getSheetFullHeightAccessibilityStringId());

        // Verifies the expected content description and accessibility string when the chrome colors
        // bottom sheet is fully expanded.
        mBottomSheetContent.setCurrentBottomSheetTypeSupplierForTesting(() -> CHROME_COLORS);
        assertEquals(
                "Chrome Colors bottom sheet",
                mBottomSheetContent.getSheetContentDescription(mContext));
        Assert.assertEquals(
                R.string.ntp_customization_chrome_colors_bottom_sheet_opened_full,
                mBottomSheetContent.getSheetFullHeightAccessibilityStringId());
        Assert.assertEquals(
                R.string.ntp_customization_chrome_colors_bottom_sheet_opened_half,
                mBottomSheetContent.getSheetHalfHeightAccessibilityStringId());
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
