// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static android.view.accessibility.AccessibilityEvent.TYPE_VIEW_FOCUSED;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.ANIMATE_SUPPLEMENTARY_CONTAINER;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.BLOCK_TOUCH_INPUT;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.BOTTOM_PADDING;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.BROWSER_CONTROLS_STATE_PROVIDER;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.FETCH_VIEW_BY_INDEX_CALLBACK;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.FOCUS_TAB_INDEX_FOR_ACCESSIBILITY;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.GET_VISIBLE_RANGE_CALLBACK;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.HUB_SEARCH_BOX_VISIBILITY_SUPPLIER;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.INITIAL_SCROLL_INDEX;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.IS_CLIP_TO_PADDING;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.IS_CONTENT_SENSITIVE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.IS_NON_ZERO_Y_OFFSET;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.IS_PINNED_TAB_STRIP_ANIMATING_SUPPLIER;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.IS_SCROLLING_SUPPLIER_CALLBACK;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.IS_TABLET_OR_LANDSCAPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.MANUAL_SEARCH_BOX_ANIMATION_SUPPLIER;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.MODE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.PAGE_KEY_LISTENER;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.SEARCH_BOX_VISIBILITY_FRACTION_SUPPLIER;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.SUPPRESS_ACCESSIBILITY;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.app.Activity;
import android.graphics.Rect;
import android.os.Build;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;
import androidx.core.util.Function;
import androidx.core.util.Pair;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.OnScrollListener;

import org.chromium.base.Callback;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.SupplementaryContainerAnimationMetadata;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.animation.AnimationHandler;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.function.Supplier;

/** ViewBinder for {@link TabListRecyclerView}. */
@NullMarked
class TabListContainerViewBinder {
    private static final int PINNED_TABS_SEARCH_BOX_DURATION_MS = 250;

    @VisibleForTesting
    static final AnimationHandler sSupplementaryContainerAnimationHandler = new AnimationHandler();

    public static class ViewHolder {
        public final TabListRecyclerView mTabListRecyclerView;
        public final ImageView mPaneHairline;
        public final LinearLayout mSupplementaryContainer;
        public final @Px int mSearchBoxGapPx;

        ViewHolder(
                TabListRecyclerView tabListRecyclerView,
                ImageView paneHairline,
                LinearLayout supplementaryContainer) {
            mTabListRecyclerView = tabListRecyclerView;
            mPaneHairline = paneHairline;
            mSupplementaryContainer = supplementaryContainer;
            mSearchBoxGapPx =
                    tabListRecyclerView
                            .getResources()
                            .getDimensionPixelSize(R.dimen.hub_search_box_gap);
        }
    }

    /**
     * Bind the given model to the given view, updating the payload in propertyKey.
     *
     * @param model The model to use.
     * @param viewHolder The ViewHolder to use.
     * @param propertyKey The key for the property to update for.
     */
    public static void bind(PropertyModel model, ViewHolder viewHolder, PropertyKey propertyKey) {
        TabListRecyclerView recyclerView = viewHolder.mTabListRecyclerView;
        ImageView hairline = viewHolder.mPaneHairline;
        LinearLayout supplementaryDataContainer = viewHolder.mSupplementaryContainer;

        if (BLOCK_TOUCH_INPUT == propertyKey) {
            recyclerView.setBlockTouchInput(model.get(BLOCK_TOUCH_INPUT));
        } else if (INITIAL_SCROLL_INDEX == propertyKey) {
            int index = model.get(INITIAL_SCROLL_INDEX);
            int offset = computeOffset(recyclerView, model);
            // RecyclerView#scrollToPosition(int) behaves incorrectly first time after cold start.
            assumeNonNull((LinearLayoutManager) recyclerView.getLayoutManager())
                    .scrollToPositionWithOffset(index, offset);
        } else if (FOCUS_TAB_INDEX_FOR_ACCESSIBILITY == propertyKey) {
            int index = model.get(FOCUS_TAB_INDEX_FOR_ACCESSIBILITY);
            RecyclerView.ViewHolder selectedViewHolder =
                    recyclerView.findViewHolderForAdapterPosition(index);
            if (selectedViewHolder == null) return;
            View focusView = selectedViewHolder.itemView;
            focusView.requestFocus();
            focusView.sendAccessibilityEvent(TYPE_VIEW_FOCUSED);
        } else if (BOTTOM_PADDING == propertyKey) {
            int left = recyclerView.getPaddingLeft();
            int top = recyclerView.getPaddingTop();
            int right = recyclerView.getPaddingRight();
            int bottom = model.get(BOTTOM_PADDING);
            recyclerView.setPadding(left, top, right, bottom);
        } else if (IS_CLIP_TO_PADDING == propertyKey) {
            recyclerView.setClipToPadding(model.get(IS_CLIP_TO_PADDING));
        } else if (FETCH_VIEW_BY_INDEX_CALLBACK == propertyKey) {
            Callback<Function<Integer, View>> callback = model.get(FETCH_VIEW_BY_INDEX_CALLBACK);
            callback.onResult(
                    (Integer index) -> {
                        RecyclerView.ViewHolder viewHolderForAdapterPosition =
                                recyclerView.findViewHolderForAdapterPosition(index);
                        return viewHolderForAdapterPosition == null
                                ? null
                                : viewHolderForAdapterPosition.itemView;
                    });
        } else if (GET_VISIBLE_RANGE_CALLBACK == propertyKey) {
            Callback<Supplier<Pair<Integer, Integer>>> callback =
                    model.get(GET_VISIBLE_RANGE_CALLBACK);
            callback.onResult(
                    () -> {
                        LinearLayoutManager layoutManager =
                                (LinearLayoutManager) recyclerView.getLayoutManager();
                        assumeNonNull(layoutManager);
                        int start = layoutManager.findFirstCompletelyVisibleItemPosition();
                        int end = layoutManager.findLastCompletelyVisibleItemPosition();
                        return new Pair<>(start, end);
                    });
        } else if (IS_SCROLLING_SUPPLIER_CALLBACK == propertyKey) {
            Callback<MonotonicObservableSupplier<Boolean>> callback =
                    model.get(IS_SCROLLING_SUPPLIER_CALLBACK);
            SettableNonNullObservableSupplier<Boolean> supplier =
                    ObservableSuppliers.createNonNull(false);
            recyclerView.addOnScrollListener(
                    new OnScrollListener() {
                        @Override
                        public void onScrollStateChanged(RecyclerView recyclerView, int newState) {
                            supplier.set(newState != RecyclerView.SCROLL_STATE_IDLE);
                        }
                    });
            callback.onResult(supplier);
        } else if (IS_CONTENT_SENSITIVE == propertyKey) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM) {
                recyclerView.setContentSensitivity(
                        model.get(IS_CONTENT_SENSITIVE)
                                ? View.CONTENT_SENSITIVITY_SENSITIVE
                                : View.CONTENT_SENSITIVITY_NOT_SENSITIVE);
            }
        } else if (PAGE_KEY_LISTENER == propertyKey) {
            recyclerView.setPageKeyListenerCallback(model.get(PAGE_KEY_LISTENER));
        } else if (SUPPRESS_ACCESSIBILITY == propertyKey) {
            int important =
                    model.get(SUPPRESS_ACCESSIBILITY)
                            ? View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS
                            : View.IMPORTANT_FOR_ACCESSIBILITY_AUTO;
            recyclerView.setImportantForAccessibility(important);
        } else if (ANIMATE_SUPPLEMENTARY_CONTAINER == propertyKey) {
            if (supplementaryDataContainer == null) return;
            if (sSupplementaryContainerAnimationHandler.isAnimationPresent()) return;

            SupplementaryContainerAnimationMetadata metadata =
                    model.get(ANIMATE_SUPPLEMENTARY_CONTAINER);
            if (metadata == null) return;

            @Px int searchBoxGapPx = viewHolder.mSearchBoxGapPx;
            int targetTranslationY = metadata.shouldShowSearchBox ? searchBoxGapPx : 0;
            float containerTranslationY = supplementaryDataContainer.getTranslationY();

            // Optimization: Skip the animation if we are already at the target position
            // and an update isn't explicitly forced.
            if (!metadata.forced && containerTranslationY == targetTranslationY) {
                return;
            }

            var manualAnimationSupplier = model.get(MANUAL_SEARCH_BOX_ANIMATION_SUPPLIER);
            var hubVisibilitySupplier = model.get(HUB_SEARCH_BOX_VISIBILITY_SUPPLIER);
            var fractionSupplier = model.get(SEARCH_BOX_VISIBILITY_FRACTION_SUPPLIER);

            // Create an animator to animate the search box transitioning into and out of view.
            // During the animation the following state is updated:
            //   - The translationY of the supplementary container is set to the animated value.
            //   - A fraction of the animation completeness is computed and supplied.
            //   - A manual animation supplier is used to signal the manual control of search box
            // animation.
            //   - The hub visibility supplier is updated based on the animation direction.
            ValueAnimator animator =
                    ValueAnimator.ofFloat(containerTranslationY, targetTranslationY);

            animator.setDuration(PINNED_TABS_SEARCH_BOX_DURATION_MS)
                    .addUpdateListener(
                            animation -> {
                                float translation = (float) animation.getAnimatedValue();
                                supplementaryDataContainer.setTranslationY(translation);
                                float fraction =
                                        searchBoxGapPx > 0 ? translation / searchBoxGapPx : 0f;
                                fractionSupplier.set(fraction);
                            });
            animator.addListener(
                    new AnimatorListenerAdapter() {
                        @Override
                        public void onAnimationStart(Animator animation) {
                            manualAnimationSupplier.set(true);
                            if (metadata.shouldShowSearchBox) {
                                hubVisibilitySupplier.set(true);
                            }
                        }

                        @Override
                        public void onAnimationEnd(@NonNull Animator animation, boolean isReverse) {
                            if (!metadata.shouldShowSearchBox) {
                                hubVisibilitySupplier.set(false);
                            }
                            manualAnimationSupplier.set(false);
                        }
                    });

            // Start the animation.
            sSupplementaryContainerAnimationHandler.startAnimation(animator);
        } else if (IS_TABLET_OR_LANDSCAPE == propertyKey) {
            boolean isTabletOrLandscape = model.get(IS_TABLET_OR_LANDSCAPE);
            int paddingTop =
                    isTabletOrLandscape
                            ? 0
                            : recyclerView
                                    .getResources()
                                    .getDimensionPixelSize(R.dimen.hub_search_box_gap);
            recyclerView.setPadding(
                    recyclerView.getPaddingLeft(),
                    paddingTop,
                    recyclerView.getPaddingRight(),
                    recyclerView.getPaddingBottom());
        } else if (IS_NON_ZERO_Y_OFFSET == propertyKey) {
            updateHairlineVisibility(model, hairline);
        } else if (IS_PINNED_TAB_STRIP_ANIMATING_SUPPLIER == propertyKey) {
            NonNullObservableSupplier<Boolean> supplier =
                    model.get(IS_PINNED_TAB_STRIP_ANIMATING_SUPPLIER);
            if (supplier == null) return;
            supplier.addSyncObserverAndCallIfNonNull(
                    (unused) -> {
                        updateHairlineVisibility(model, hairline);
                    });
        }
    }

    /**
     * Update the visibility of the hairline above the tab list. The hairline is hidden when the
     * pinned tab strip is animating or when tab list is at the top position(no y offset).
     */
    private static void updateHairlineVisibility(PropertyModel model, ImageView hairline) {
        if (hairline == null) return;

        NonNullObservableSupplier<Boolean> isAnimatingSupplier =
                model.get(IS_PINNED_TAB_STRIP_ANIMATING_SUPPLIER);
        boolean isAnimating = isAnimatingSupplier != null && isAnimatingSupplier.get();
        boolean isYOffsetNonZero = model.get(IS_NON_ZERO_Y_OFFSET);
        boolean shouldBeVisible = isYOffsetNonZero && !isAnimating;

        hairline.setVisibility(shouldBeVisible ? View.VISIBLE : View.GONE);
    }

    private static int computeOffset(TabListRecyclerView view, PropertyModel model) {
        int width = view.getWidth();
        int height = view.getHeight();
        final BrowserControlsStateProvider browserControlsStateProvider =
                model.get(BROWSER_CONTROLS_STATE_PROVIDER);
        // If layout hasn't happened yet fallback to dimensions based on visible display frame. This
        // works for multi-window and different orientations. Don't use View#post() because this
        // will cause animation jank for expand/shrink animations.
        if (width == 0 || height == 0) {
            Rect frame = new Rect();
            ((Activity) view.getContext())
                    .getWindow()
                    .getDecorView()
                    .getWindowVisibleDisplayFrame(frame);
            width = frame.width();
            // Remove toolbar height from height.
            height =
                    frame.height()
                            - Math.round(browserControlsStateProvider.getTopVisibleContentOffset());
        }
        if (width <= 0 || height <= 0) return 0;

        LinearLayoutManager layoutManager = (LinearLayoutManager) view.getLayoutManager();
        assert model.get(MODE) == TabListMode.GRID;
        GridLayoutManager gridLayoutManager = (GridLayoutManager) layoutManager;
        assumeNonNull(gridLayoutManager);
        int cardWidth = width / gridLayoutManager.getSpanCount();
        int cardHeight =
                TabUtils.deriveGridCardHeight(
                        cardWidth, view.getContext(), browserControlsStateProvider);
        return Math.max(0, height / 2 - cardHeight / 2);
    }
}
