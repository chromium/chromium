// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_promo;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.view.View;
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
    @Mock private SafetyPromoCarouselMediator mMockMediator;

    private Context mContext;
    private SafetyPromoCarouselCoordinator mCoordinator;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mContext.setTheme(R.style.Theme_BrowserUI_DayNight);
    }

    @Test
    public void testInitialization() {
        createCoordinator(/* useLandscapeLayout= */ false, ObservableSuppliers.createNullable());

        RecyclerView recyclerView = mCoordinator.getView().getRecyclerView();
        assertNotNull(recyclerView.getAdapter());
        assertEquals(3, recyclerView.getAdapter().getItemCount());

        assertTrue(recyclerView.getLayoutManager() instanceof LinearLayoutManager);
        LinearLayoutManager layoutManager = (LinearLayoutManager) recyclerView.getLayoutManager();
        assertEquals(LinearLayoutManager.HORIZONTAL, layoutManager.getOrientation());

        assertItemLayout(R.id.carousel_illustration);
    }

    @Test
    public void testInitialization_landscape() {
        createCoordinator(
                /* useLandscapeLayout= */ true,
                ObservableSuppliers.createNullable(SafetyPromoItem.INCOGNITO));

        RecyclerView recyclerView = mCoordinator.getView().getRecyclerView();
        assertNotNull(recyclerView.getAdapter());
        assertEquals(TEST_ITEMS.size(), recyclerView.getAdapter().getItemCount());
        assertItemLayout(R.id.carousel_landscape_illustration_container);

        mCoordinator.getView().findViewById(R.id.fre_continue_button).performClick();
        verify(mAdvancePage).run();
    }

    @Test
    public void testDestroy() {
        createCoordinator(/* useLandscapeLayout= */ false, ObservableSuppliers.createNullable());

        mCoordinator.destroy();

        verify(mMockMediator).destroy();
    }

    @Test
    public void testScrollListener_reportsSnappedPageWhenIdle() {
        createCoordinator(/* useLandscapeLayout= */ false, ObservableSuppliers.createNullable());

        SafetyPromoCarouselView view = mCoordinator.getView();
        layoutView(view);
        RecyclerView recyclerView = view.getRecyclerView();

        RecyclerView.OnScrollListener listener = mCoordinator.getOnScrollListenerForTesting();
        listener.onScrollStateChanged(recyclerView, RecyclerView.SCROLL_STATE_SETTLING);
        verify(mMockMediator, never()).onPageSnapped(anyInt());

        listener.onScrollStateChanged(recyclerView, RecyclerView.SCROLL_STATE_IDLE);
        verify(mMockMediator).onPageSnapped(0);
    }

    @Test
    public void testContinueButton_triggersCallback() {
        createCoordinator(/* useLandscapeLayout= */ false, ObservableSuppliers.createNullable());

        mCoordinator.getView().findViewById(R.id.fre_continue_button).performClick();

        verify(mAdvancePage).run();
    }

    @Test
    public void testSwitchLayout_keepsCurrentItem() {
        SettableNullableObservableSupplier<SafetyPromoItem> supplier =
                ObservableSuppliers.createNullable(SafetyPromoItem.PASSWORD_MANAGER);
        createCoordinatorWithRealMediator(/* useLandscapeLayout= */ false, supplier);
        supplier.set(SafetyPromoItem.INCOGNITO);
        // Clear the supplier so a kept item can be told apart from one re-read from the supplier.
        supplier.set(null);

        rotate(/* useLandscapeLayout= */ true);

        RecyclerView recyclerView = mCoordinator.getView().getRecyclerView();
        assertNotNull(recyclerView.getAdapter());
        assertEquals(TEST_ITEMS.size(), recyclerView.getAdapter().getItemCount());
        assertSelectedItemState(SafetyPromoItem.INCOGNITO);
        assertVisibleIllustration(SafetyPromoItem.INCOGNITO);
        assertItemLayout(R.id.carousel_landscape_illustration_container);
    }

    @Test
    public void testSwitchLayoutBack_reusesCachedView() {
        createCoordinator(/* useLandscapeLayout= */ false, ObservableSuppliers.createNullable());
        SafetyPromoCarouselView portraitView = mCoordinator.getView();
        RecyclerView.Adapter<?> portraitAdapter = portraitView.getRecyclerView().getAdapter();

        rotate(/* useLandscapeLayout= */ true);
        SafetyPromoCarouselView landscapeView = mCoordinator.getView();
        assertNotSame(portraitView, landscapeView);
        rotate(/* useLandscapeLayout= */ false);

        assertSame(portraitView, mCoordinator.getView());
        assertSame(portraitAdapter, mCoordinator.getView().getRecyclerView().getAdapter());

        rotate(/* useLandscapeLayout= */ true);

        assertSame(landscapeView, mCoordinator.getView());
    }

    @Test
    public void testSwitchLayoutBack_restoresStateChangedWhileHidden() {
        SettableNullableObservableSupplier<SafetyPromoItem> supplier =
                ObservableSuppliers.createNullable(SafetyPromoItem.PASSWORD_MANAGER);
        createCoordinatorWithRealMediator(/* useLandscapeLayout= */ false, supplier);
        SafetyPromoCarouselView portraitView = mCoordinator.getView();

        rotate(/* useLandscapeLayout= */ true);
        supplier.set(SafetyPromoItem.ENHANCED_SAFE_BROWSING);

        assertSelectedItemState(SafetyPromoItem.ENHANCED_SAFE_BROWSING);
        // The hidden view stays bound to the model.
        assertTitle(portraitView, SafetyPromoItem.ENHANCED_SAFE_BROWSING);

        rotate(/* useLandscapeLayout= */ false);

        assertSame(portraitView, mCoordinator.getView());
        assertSelectedItemState(SafetyPromoItem.ENHANCED_SAFE_BROWSING);
        assertVisibleIllustration(SafetyPromoItem.ENHANCED_SAFE_BROWSING);
    }

    @Test
    public void testSwitchLayout_inflatesNewView() {
        createCoordinator(/* useLandscapeLayout= */ false, ObservableSuppliers.createNullable());
        SafetyPromoCarouselView portraitView = mCoordinator.getView();

        rotate(/* useLandscapeLayout= */ true);

        assertNotSame(portraitView, mCoordinator.getView());
    }

    @Test
    public void testConfigurationChange_resourcesChanged_recreatesViews() {
        createCoordinator(/* useLandscapeLayout= */ false, ObservableSuppliers.createNullable());
        SafetyPromoCarouselView oldPortraitView = mCoordinator.getView();

        changeResources(/* useLandscapeLayout= */ false);

        assertNotSame(oldPortraitView, mCoordinator.getView());
    }

    @Test
    public void testConfigurationChange_layoutSwitchAndResourcesChanged_inflatesNewView() {
        createCoordinator(/* useLandscapeLayout= */ false, ObservableSuppliers.createNullable());
        rotate(/* useLandscapeLayout= */ true);
        SafetyPromoCarouselView oldLandscapeView = mCoordinator.getView();
        rotate(/* useLandscapeLayout= */ false);
        SafetyPromoCarouselView oldPortraitView = mCoordinator.getView();

        changeResources(/* useLandscapeLayout= */ true);

        assertNotSame(oldLandscapeView, mCoordinator.getView());

        rotate(/* useLandscapeLayout= */ false);
        assertNotSame(oldPortraitView, mCoordinator.getView());
    }

    @Test
    public void testConfigurationChange_rotationWithoutLayoutSwitch_keepsView() {
        createCoordinator(/* useLandscapeLayout= */ false, ObservableSuppliers.createNullable());
        SafetyPromoCarouselView portraitView = mCoordinator.getView();

        rotate(/* useLandscapeLayout= */ false);

        assertSame(portraitView, mCoordinator.getView());
    }

    private void createCoordinator(
            boolean useLandscapeLayout,
            SettableNullableObservableSupplier<SafetyPromoItem> supplier) {
        mCoordinator =
                new SafetyPromoCarouselCoordinator(
                        mContext, useLandscapeLayout, supplier, mAdvancePage, TEST_ITEMS);
        mCoordinator.setMediatorForTesting(mMockMediator);
    }

    private void createCoordinatorWithRealMediator(
            boolean useLandscapeLayout,
            SettableNullableObservableSupplier<SafetyPromoItem> supplier) {
        mCoordinator =
                new SafetyPromoCarouselCoordinator(
                        mContext, useLandscapeLayout, supplier, mAdvancePage, TEST_ITEMS);
    }

    private void rotate(boolean useLandscapeLayout) {
        mCoordinator.onConfigurationChanged(useLandscapeLayout, /* resourcesChanged= */ false);
    }

    private void changeResources(boolean useLandscapeLayout) {
        mCoordinator.onConfigurationChanged(useLandscapeLayout, /* resourcesChanged= */ true);
    }

    private void assertTitle(SafetyPromoCarouselView view, SafetyPromoItem item) {
        TextView titleView = view.findViewById(R.id.safety_promo_carousel_title);
        assertEquals(mContext.getString(item.carouselTitleResId), titleView.getText().toString());
    }

    private void assertSelectedItemState(SafetyPromoItem item) {
        SafetyPromoCarouselView view = mCoordinator.getView();
        assertTitle(view, item);
        TextView subtitleView = view.findViewById(R.id.safety_promo_carousel_subtitle);
        assertEquals(
                mContext.getString(item.carouselSubtitleResId), subtitleView.getText().toString());

        SafetyPromoPageIndicatorView indicatorView =
                view.findViewById(R.id.safety_promo_carousel_page_indicator);
        assertEquals(TEST_ITEMS.size(), indicatorView.getPageCountForTesting());
        assertEquals(TEST_ITEMS.indexOf(item), indicatorView.getActivePositionForTesting());
    }

    /** Lays out the shown view and asserts the carousel shows the illustration for {@code item}. */
    private void assertVisibleIllustration(SafetyPromoItem item) {
        SafetyPromoCarouselView view = mCoordinator.getView();
        layoutView(view);
        LinearLayoutManager layoutManager =
                (LinearLayoutManager) view.getRecyclerView().getLayoutManager();
        assertEquals(TEST_ITEMS.indexOf(item), layoutManager.findFirstVisibleItemPosition());
    }

    private static void layoutView(SafetyPromoCarouselView view) {
        view.measure(
                View.MeasureSpec.makeMeasureSpec(1200, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(800, View.MeasureSpec.EXACTLY));
        view.layout(0, 0, 1200, 800);
    }

    private void assertItemLayout(int rootId) {
        RecyclerView recyclerView = mCoordinator.getView().getRecyclerView();
        SafetyPromoCarouselAdapter adapter = (SafetyPromoCarouselAdapter) recyclerView.getAdapter();
        SafetyPromoCarouselAdapter.ViewHolder holder =
                adapter.onCreateViewHolder(recyclerView, /* viewType= */ 0);
        assertEquals(rootId, holder.itemView.getId());
    }
}
