// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_promo;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.view.LayoutInflater;
import android.widget.TextView;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.List;

/** Unit tests for {@link SafetyPromoCarouselCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SafetyPromoCarouselCoordinatorUnitTest {
    private static final List<SafetyPromoItem> TEST_ITEMS =
            List.of(
                    SafetyPromoItem.PASSWORD_MANAGER,
                    SafetyPromoItem.ENHANCED_SAFE_BROWSING,
                    SafetyPromoItem.INCOGNITO);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Runnable mAdvancePage;

    private Context mContext;
    private SafetyPromoCarouselView mView;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mContext.setTheme(R.style.Theme_BrowserUI_DayNight);
        mView =
                (SafetyPromoCarouselView)
                        LayoutInflater.from(mContext)
                                .inflate(
                                        R.layout.safety_promo_fre_carousel_portrait_view,
                                        /* root= */ null);
    }

    @Test
    public void testInitialization() {
        new SafetyPromoCarouselCoordinator(
                mContext, mView, ObservableSuppliers.createNullable(), mAdvancePage, TEST_ITEMS);

        RecyclerView recyclerView = mView.getRecyclerView();
        assertNotNull(recyclerView.getAdapter());
        assertEquals(3, recyclerView.getAdapter().getItemCount());

        assertTrue(recyclerView.getLayoutManager() instanceof LinearLayoutManager);
        LinearLayoutManager layoutManager = (LinearLayoutManager) recyclerView.getLayoutManager();
        assertEquals(LinearLayoutManager.HORIZONTAL, layoutManager.getOrientation());

        assertSelectedItemState(SafetyPromoItem.PASSWORD_MANAGER);
    }

    @Test
    public void testSelectedItem_appliedBeforeFirstDraw() {
        SettableNullableObservableSupplier<SafetyPromoItem> supplier =
                ObservableSuppliers.createNullable(SafetyPromoItem.INCOGNITO);
        new SafetyPromoCarouselCoordinator(mContext, mView, supplier, mAdvancePage, TEST_ITEMS);

        assertSelectedItemState(SafetyPromoItem.INCOGNITO);
    }

    @Test
    public void testSelectedItemSetAfterConstruction_updatesSelectedItemState() {
        SettableNullableObservableSupplier<SafetyPromoItem> supplier =
                ObservableSuppliers.createNullable();
        new SafetyPromoCarouselCoordinator(mContext, mView, supplier, mAdvancePage, TEST_ITEMS);

        assertSelectedItemState(SafetyPromoItem.PASSWORD_MANAGER);

        supplier.set(SafetyPromoItem.INCOGNITO);

        assertSelectedItemState(SafetyPromoItem.INCOGNITO);
    }

    @Test
    public void testSelectedItemChanged_updatesSelectedItemState() {
        SettableNullableObservableSupplier<SafetyPromoItem> supplier =
                ObservableSuppliers.createNullable(SafetyPromoItem.PASSWORD_MANAGER);
        new SafetyPromoCarouselCoordinator(mContext, mView, supplier, mAdvancePage, TEST_ITEMS);

        supplier.set(SafetyPromoItem.ENHANCED_SAFE_BROWSING);

        assertSelectedItemState(SafetyPromoItem.ENHANCED_SAFE_BROWSING);
    }

    @Test
    public void testDestroy_stopsObservingSelectedItem() {
        SettableNullableObservableSupplier<SafetyPromoItem> supplier =
                ObservableSuppliers.createNullable(SafetyPromoItem.PASSWORD_MANAGER);
        SafetyPromoCarouselCoordinator coordinator =
                new SafetyPromoCarouselCoordinator(
                        mContext, mView, supplier, mAdvancePage, TEST_ITEMS);

        coordinator.destroy();
        supplier.set(SafetyPromoItem.ENHANCED_SAFE_BROWSING);

        assertSelectedItemState(SafetyPromoItem.PASSWORD_MANAGER);
    }

    @Test
    public void testContinueButton_triggersCallback() {
        new SafetyPromoCarouselCoordinator(
                mContext, mView, ObservableSuppliers.createNullable(), mAdvancePage, TEST_ITEMS);

        mView.findViewById(R.id.fre_continue_button).performClick();

        verify(mAdvancePage).run();
    }

    private void assertSelectedItemState(SafetyPromoItem item) {
        TextView titleView = mView.findViewById(R.id.safety_promo_carousel_title);
        assertEquals(mContext.getString(item.carouselTitleResId), titleView.getText().toString());
        TextView subtitleView = mView.findViewById(R.id.safety_promo_carousel_subtitle);
        assertEquals(
                mContext.getString(item.carouselSubtitleResId), subtitleView.getText().toString());

        SafetyPromoPageIndicatorView indicatorView =
                mView.findViewById(R.id.safety_promo_carousel_page_indicator);
        assertEquals(TEST_ITEMS.size(), indicatorView.getPageCountForTesting());
        assertEquals(TEST_ITEMS.indexOf(item), indicatorView.getActivePositionForTesting());
    }
}
