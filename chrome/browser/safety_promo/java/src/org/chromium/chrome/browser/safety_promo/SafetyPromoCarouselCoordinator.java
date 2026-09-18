// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_promo;

import static org.chromium.chrome.browser.safety_promo.SafetyPromoCarouselProperties.ON_CONTINUE_CLICKED;
import static org.chromium.chrome.browser.safety_promo.SafetyPromoCarouselProperties.SUBTITLE_RES_ID;
import static org.chromium.chrome.browser.safety_promo.SafetyPromoCarouselProperties.TITLE_RES_ID;

import android.content.Context;
import android.view.View;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.PagerSnapHelper;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.List;

/** Coordinator for Safety Promo Carousel. */
@NullMarked
public class SafetyPromoCarouselCoordinator {
    private final PropertyModel mModel;
    private final RecyclerView mRecyclerView;
    private final NullableObservableSupplier<SafetyPromoItem> mSelectedItemSupplier;
    private final Callback<@Nullable SafetyPromoItem> mSelectedItemObserver;
    private final List<SafetyPromoItem> mItems;

    /**
     * @param context The {@link Context} used for inflating views and accessing resources.
     * @param view The {@link SafetyPromoCarouselView} that displays the promo carousel.
     * @param selectedItemSupplier Supplier of the {@link SafetyPromoItem} picked on the overview
     *     page.
     * @param advancePage The {@link Runnable} to execute when advancing to the next page or
     *     completing the promo.
     * @param items The list of {@link SafetyPromoItem}s to display in the carousel.
     */
    public SafetyPromoCarouselCoordinator(
            Context context,
            SafetyPromoCarouselView view,
            NullableObservableSupplier<SafetyPromoItem> selectedItemSupplier,
            Runnable advancePage,
            List<SafetyPromoItem> items) {
        // The carousel promo should only be displayed when promo items are configured.
        assert !items.isEmpty();

        mSelectedItemSupplier = selectedItemSupplier;
        mSelectedItemObserver = this::onSelectedItemChanged;
        mItems = items;
        mRecyclerView = view.getRecyclerView();
        mModel =
                new PropertyModel.Builder(SafetyPromoCarouselProperties.ALL_KEYS)
                        .with(ON_CONTINUE_CLICKED, _ -> advancePage.run())
                        .build();

        initializeRecyclerView(context);
        setCurrentItem(getPositionForItem(selectedItemSupplier.get()));

        PropertyModelChangeProcessor.create(mModel, view, SafetyPromoCarouselViewBinder::bind);
        selectedItemSupplier.addSyncObserver(mSelectedItemObserver);
    }

    public void destroy() {
        mSelectedItemSupplier.removeObserver(mSelectedItemObserver);
    }

    private void initializeRecyclerView(Context context) {
        LinearLayoutManager layoutManager =
                new LinearLayoutManager(context, LinearLayoutManager.HORIZONTAL, false);
        mRecyclerView.setLayoutManager(layoutManager);
        mRecyclerView.setAdapter(new SafetyPromoCarouselAdapter(mItems));

        PagerSnapHelper snapHelper = new PagerSnapHelper();
        snapHelper.attachToRecyclerView(mRecyclerView);

        mRecyclerView.addOnScrollListener(
                new RecyclerView.OnScrollListener() {
                    @Override
                    public void onScrollStateChanged(RecyclerView recyclerView, int newState) {
                        super.onScrollStateChanged(recyclerView, newState);
                        if (newState != RecyclerView.SCROLL_STATE_IDLE) return;

                        View centerView = snapHelper.findSnapView(layoutManager);
                        if (centerView == null) return;

                        int position = layoutManager.getPosition(centerView);
                        if (position == RecyclerView.NO_POSITION) return;

                        updateHeader(position);
                    }
                });
    }

    private void onSelectedItemChanged(@Nullable SafetyPromoItem item) {
        if (item == null) return;

        setCurrentItem(getPositionForItem(item));
    }

    private int getPositionForItem(@Nullable SafetyPromoItem item) {
        if (item == null) return 0;

        int position = mItems.indexOf(item);
        return Math.max(0, position);
    }

    private void setCurrentItem(int position) {
        updateHeader(position);
        mRecyclerView.scrollToPosition(position);
    }

    private void updateHeader(int position) {
        assert position >= 0 && position < mItems.size();

        SafetyPromoItem item = mItems.get(position);
        mModel.set(TITLE_RES_ID, item.carouselTitleResId);
        mModel.set(SUBTITLE_RES_ID, item.carouselSubtitleResId);
    }
}
