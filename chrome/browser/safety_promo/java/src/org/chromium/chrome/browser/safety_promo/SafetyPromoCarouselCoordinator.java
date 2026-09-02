// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_promo;

import static org.chromium.chrome.browser.safety_promo.SafetyPromoCarouselProperties.ON_CONTINUE_CLICKED;
import static org.chromium.chrome.browser.safety_promo.SafetyPromoCarouselProperties.SUBTITLE_RES_ID;
import static org.chromium.chrome.browser.safety_promo.SafetyPromoCarouselProperties.TITLE_RES_ID;

import android.content.Context;
import android.view.View;

import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.PagerSnapHelper;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.List;

/** Coordinator for Safety Promo Carousel. */
@NullMarked
public class SafetyPromoCarouselCoordinator {
    private final PropertyModel mModel;
    private final List<SafetyPromoItem> mItems;
    private int mCurrentPosition;

    /**
     * @param context The {@link Context} used for inflating views and accessing resources.
     * @param view The {@link SafetyPromoCarouselView} that displays the promo carousel.
     * @param advancePage The {@link Runnable} to execute when advancing to the next page or
     *     completing the promo.
     * @param items The list of {@link SafetyPromoItem}s to display in the carousel.
     */
    public SafetyPromoCarouselCoordinator(
            Context context,
            SafetyPromoCarouselView view,
            Runnable advancePage,
            List<SafetyPromoItem> items) {
        mItems = items;
        mCurrentPosition = 0;

        RecyclerView recyclerView = view.getRecyclerView();
        LinearLayoutManager layoutManager =
                new LinearLayoutManager(context, LinearLayoutManager.HORIZONTAL, false);
        recyclerView.setLayoutManager(layoutManager);
        SafetyPromoCarouselAdapter adapter = new SafetyPromoCarouselAdapter(items);
        recyclerView.setAdapter(adapter);

        // The carousel promo should only be displayed when promo items are configured.
        assert !items.isEmpty();
        SafetyPromoItem initialItem = items.get(0);

        mModel =
                new PropertyModel.Builder(SafetyPromoCarouselProperties.ALL_KEYS)
                        .with(TITLE_RES_ID, initialItem.carouselTitleResId)
                        .with(SUBTITLE_RES_ID, initialItem.carouselSubtitleResId)
                        .with(ON_CONTINUE_CLICKED, v -> advancePage.run())
                        .build();

        PropertyModelChangeProcessor.create(mModel, view, SafetyPromoCarouselViewBinder::bind);

        PagerSnapHelper snapHelper = new PagerSnapHelper();
        snapHelper.attachToRecyclerView(recyclerView);
        recyclerView.addOnScrollListener(
                new RecyclerView.OnScrollListener() {
                    @Override
                    public void onScrollStateChanged(RecyclerView recyclerView, int newState) {
                        super.onScrollStateChanged(recyclerView, newState);
                        if (newState == RecyclerView.SCROLL_STATE_IDLE) {
                            View centerView = snapHelper.findSnapView(layoutManager);
                            if (centerView != null) {
                                int position = layoutManager.getPosition(centerView);
                                if (position != RecyclerView.NO_POSITION
                                        && position != mCurrentPosition) {
                                    mCurrentPosition = position;
                                    updateHeaderForPosition(mCurrentPosition);
                                }
                            }
                        }
                    }
                });
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void updateHeaderForPosition(int position) {
        if (position >= 0 && position < mItems.size()) {
            SafetyPromoItem item = mItems.get(position);
            mModel.set(TITLE_RES_ID, item.carouselTitleResId);
            mModel.set(SUBTITLE_RES_ID, item.carouselSubtitleResId);
        }
    }
}
