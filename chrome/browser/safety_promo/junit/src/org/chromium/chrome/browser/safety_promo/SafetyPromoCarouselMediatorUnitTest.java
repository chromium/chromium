// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_promo;

import static org.junit.Assert.assertEquals;

import static org.chromium.chrome.browser.safety_promo.SafetyPromoCarouselProperties.ACTIVE_PAGE_INDEX;
import static org.chromium.chrome.browser.safety_promo.SafetyPromoCarouselProperties.PAGE_COUNT;
import static org.chromium.chrome.browser.safety_promo.SafetyPromoCarouselProperties.SUBTITLE_RES_ID;
import static org.chromium.chrome.browser.safety_promo.SafetyPromoCarouselProperties.TITLE_RES_ID;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Unit tests for {@link SafetyPromoCarouselMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SafetyPromoCarouselMediatorUnitTest {
    private static final List<SafetyPromoItem> TEST_ITEMS =
            List.of(
                    SafetyPromoItem.PASSWORD_MANAGER,
                    SafetyPromoItem.ENHANCED_SAFE_BROWSING,
                    SafetyPromoItem.INCOGNITO);

    private PropertyModel mModel;
    private SettableNullableObservableSupplier<SafetyPromoItem> mSelectedItemSupplier;
    private SafetyPromoCarouselMediator mMediator;

    @Before
    public void setUp() {
        mModel = new PropertyModel.Builder(SafetyPromoCarouselProperties.ALL_KEYS).build();
        mSelectedItemSupplier = ObservableSuppliers.createNullable();
    }

    @Test
    public void testConstruction_setsPageCount() {
        createMediator();

        assertEquals(TEST_ITEMS.size(), mModel.get(PAGE_COUNT));
    }

    @Test
    public void testConstruction_noSelectedItem_activatesFirstPage() {
        createMediator();

        assertActivePage(SafetyPromoItem.PASSWORD_MANAGER);
    }

    @Test
    public void testConstruction_selectedItem_activatesItsPage() {
        mSelectedItemSupplier.set(SafetyPromoItem.INCOGNITO);

        createMediator();

        assertActivePage(SafetyPromoItem.INCOGNITO);
    }

    @Test
    public void testConstruction_selectedItemNotInCarousel_activatesFirstPage() {
        mSelectedItemSupplier.set(SafetyPromoItem.HISTORY_QUICK_DELETE);

        createMediator();

        assertActivePage(SafetyPromoItem.PASSWORD_MANAGER);
    }

    @Test
    public void testSelectedItemChanged_activatesItsPage() {
        createMediator();

        mSelectedItemSupplier.set(SafetyPromoItem.ENHANCED_SAFE_BROWSING);

        assertActivePage(SafetyPromoItem.ENHANCED_SAFE_BROWSING);
    }

    @Test
    public void testSelectedItemCleared_keepsActivePage() {
        mSelectedItemSupplier.set(SafetyPromoItem.INCOGNITO);
        createMediator();

        mSelectedItemSupplier.set(null);

        assertActivePage(SafetyPromoItem.INCOGNITO);
    }

    @Test
    public void testOnPageSnapped_activatesPage() {
        createMediator();

        mMediator.onPageSnapped(2);

        assertActivePage(SafetyPromoItem.INCOGNITO);
    }

    @Test
    public void testDestroy_stopsObservingSelectedItem() {
        createMediator();

        mMediator.destroy();
        mSelectedItemSupplier.set(SafetyPromoItem.INCOGNITO);

        assertActivePage(SafetyPromoItem.PASSWORD_MANAGER);
    }

    private void createMediator() {
        mMediator = new SafetyPromoCarouselMediator(mModel, mSelectedItemSupplier, TEST_ITEMS);
    }

    private void assertActivePage(SafetyPromoItem item) {
        assertEquals(TEST_ITEMS.indexOf(item), mModel.get(ACTIVE_PAGE_INDEX));
        assertEquals(item.carouselTitleResId, mModel.get(TITLE_RES_ID));
        assertEquals(item.carouselSubtitleResId, mModel.get(SUBTITLE_RES_ID));
    }
}
