// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_promo;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.safety_promo.SafetyPromoCarouselProperties.ON_CONTINUE_CLICKED;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.LayoutRes;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.PagerSnapHelper;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.OnScrollListener;

import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.List;

/**
 * Coordinator for Safety Promo Carousel. Owns the model, the mediator and the views, and wires them
 * together.
 *
 * <p>The portrait and landscape views are inflated lazily and cached. Each cached view is bound to
 * the model by its own change processor, so a hidden view stays up to date, including the page it
 * is scrolled to. {@link #getView()} returns the view of the current layout, which the embedder
 * adds to the view hierarchy. Switching back to a layout that was shown before reuses its view
 * instead of re-inflating it or recreating its RecyclerView.
 */
@NullMarked
public class SafetyPromoCarouselCoordinator {
    private final Context mContext;
    private final PropertyModel mModel;
    private SafetyPromoCarouselMediator mMediator;
    private final List<SafetyPromoItem> mItems;
    private @Nullable SafetyPromoCarouselView mPortraitView;
    private @Nullable SafetyPromoCarouselView mLandscapeView;
    private @Nullable
            PropertyModelChangeProcessor<PropertyModel, SafetyPromoCarouselView, PropertyKey>
            mPortraitChangeProcessor;
    private @Nullable
            PropertyModelChangeProcessor<PropertyModel, SafetyPromoCarouselView, PropertyKey>
            mLandscapeChangeProcessor;
    private @Nullable OnScrollListener mPortraitScrollListener;
    private @Nullable OnScrollListener mLandscapeScrollListener;
    private boolean mUseLandscapeLayout;

    /**
     * @param context The {@link Context} used for inflating views and accessing resources.
     * @param useLandscapeLayout Whether to show the landscape (dual-pane) layout.
     * @param selectedItemSupplier Supplier of the {@link SafetyPromoItem} picked on the overview
     *     page.
     * @param advancePage The {@link Runnable} to execute when advancing to the next page or
     *     completing the promo.
     * @param items The list of {@link SafetyPromoItem}s to display in the carousel.
     */
    public SafetyPromoCarouselCoordinator(
            Context context,
            boolean useLandscapeLayout,
            NullableObservableSupplier<SafetyPromoItem> selectedItemSupplier,
            Runnable advancePage,
            List<SafetyPromoItem> items) {
        // The carousel promo should only be displayed when promo items are configured.
        assert !items.isEmpty();

        mContext = context;
        mItems = items;
        mModel =
                new PropertyModel.Builder(SafetyPromoCarouselProperties.ALL_KEYS)
                        .with(ON_CONTINUE_CLICKED, _ -> advancePage.run())
                        .build();

        mUseLandscapeLayout = useLandscapeLayout;
        mMediator = new SafetyPromoCarouselMediator(mModel, selectedItemSupplier, mItems);
        getOrCreateView();
    }

    /** Stops observing the selected item and unbinds and drops the carousel views. */
    public void destroy() {
        mMediator.destroy();
        clearCachedViews();
    }

    /**
     * Updates the carousel after a configuration change, keeping the current item. The embedder
     * should then show {@link #getView()}, which may have changed.
     *
     * @param useLandscapeLayout Whether to show the landscape (dual-pane) layout.
     * @param resourcesChanged Whether the change may outdate the resources of the cached views, in
     *     which case they are dropped.
     */
    public void onConfigurationChanged(boolean useLandscapeLayout, boolean resourcesChanged) {
        if (!resourcesChanged && useLandscapeLayout == mUseLandscapeLayout) return;
        if (resourcesChanged) clearCachedViews();

        mUseLandscapeLayout = useLandscapeLayout;
        getOrCreateView();
    }

    /** Returns the carousel view of the current layout. */
    public SafetyPromoCarouselView getView() {
        return assumeNonNull(mUseLandscapeLayout ? mLandscapeView : mPortraitView);
    }

    /**
     * Returns the view of the current layout, inflating and binding it if it isn't cached. Binding
     * scrolls a new view to the active page.
     */
    private SafetyPromoCarouselView getOrCreateView() {
        SafetyPromoCarouselView view = mUseLandscapeLayout ? mLandscapeView : mPortraitView;
        if (view != null) return view;

        @LayoutRes
        int layoutId =
                mUseLandscapeLayout
                        ? R.layout.safety_promo_fre_carousel_landscape_view
                        : R.layout.safety_promo_fre_carousel_portrait_view;
        @LayoutRes
        int itemLayoutId =
                mUseLandscapeLayout
                        ? R.layout.safety_promo_carousel_landscape_illustration
                        : R.layout.safety_promo_carousel_portrait_illustration;
        view =
                (SafetyPromoCarouselView)
                        LayoutInflater.from(mContext)
                                .inflate(layoutId, /* root= */ null, /* attachToRoot= */ false);
        var scrollListener = initializeRecyclerView(view.getRecyclerView(), itemLayoutId);
        var changeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, view, SafetyPromoCarouselViewBinder::bind);

        if (mUseLandscapeLayout) {
            mLandscapeView = view;
            mLandscapeChangeProcessor = changeProcessor;
            mLandscapeScrollListener = scrollListener;
        } else {
            mPortraitView = view;
            mPortraitChangeProcessor = changeProcessor;
            mPortraitScrollListener = scrollListener;
        }
        return view;
    }

    /**
     * Unbinds the model and drops the cached views, so the next {@link #getOrCreateView} inflates a
     * new view. Doesn't remove the views from the view hierarchy.
     */
    private void clearCachedViews() {
        if (mPortraitChangeProcessor != null) {
            mPortraitChangeProcessor.destroy();
            mPortraitChangeProcessor = null;
        }
        if (mLandscapeChangeProcessor != null) {
            mLandscapeChangeProcessor.destroy();
            mLandscapeChangeProcessor = null;
        }
        mPortraitView = null;
        mLandscapeView = null;
        mPortraitScrollListener = null;
        mLandscapeScrollListener = null;
    }

    private OnScrollListener initializeRecyclerView(
            RecyclerView recyclerView, @LayoutRes int itemLayoutId) {
        LinearLayoutManager layoutManager =
                new LinearLayoutManager(
                        recyclerView.getContext(),
                        LinearLayoutManager.HORIZONTAL,
                        /* reverseLayout= */ false);
        recyclerView.setLayoutManager(layoutManager);
        recyclerView.setAdapter(new SafetyPromoCarouselAdapter(mItems, itemLayoutId));

        PagerSnapHelper snapHelper = new PagerSnapHelper();
        snapHelper.attachToRecyclerView(recyclerView);

        OnScrollListener scrollListener =
                new OnScrollListener() {
                    @Override
                    public void onScrollStateChanged(RecyclerView recyclerView, int newState) {
                        super.onScrollStateChanged(recyclerView, newState);
                        if (newState != RecyclerView.SCROLL_STATE_IDLE) return;

                        View centerView = snapHelper.findSnapView(layoutManager);
                        if (centerView == null) return;

                        int position = layoutManager.getPosition(centerView);
                        if (position == RecyclerView.NO_POSITION) return;

                        mMediator.onPageSnapped(position);
                    }
                };
        recyclerView.addOnScrollListener(scrollListener);
        return scrollListener;
    }

    void setMediatorForTesting(SafetyPromoCarouselMediator mediator) {
        mMediator = mediator;
    }

    OnScrollListener getOnScrollListenerForTesting() {
        return assumeNonNull(
                mUseLandscapeLayout ? mLandscapeScrollListener : mPortraitScrollListener);
    }
}
