// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.search;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Point;
import android.graphics.Rect;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnDragListener;
import android.view.ViewGroup;

import androidx.annotation.StyleRes;

import org.chromium.base.MathUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.composeplate.ComposeplateUtils;
import org.chromium.chrome.browser.feed.FeedSurfaceScrollDelegate;
import org.chromium.chrome.browser.lens.LensController;
import org.chromium.chrome.browser.lens.LensEntryPoint;
import org.chromium.chrome.browser.lens.LensIntentParams;
import org.chromium.chrome.browser.lens.LensQueryParams;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Supplier;

@NullMarked
class SearchBoxMediator implements DestroyObserver {
    private final Context mContext;
    private final PropertyModel mModel;
    private final ViewGroup mView;
    private final List<OnClickListener> mVoiceSearchClickListeners = new ArrayList<>();
    private final List<OnClickListener> mLensClickListeners = new ArrayList<>();
    private final float mTransitionEndOffset;
    private @Nullable ActivityLifecycleDispatcher mActivityLifecycleDispatcher;

    /** Constructor. */
    SearchBoxMediator(Context context, PropertyModel model, ViewGroup view, boolean isTablet) {
        mContext = context;
        mModel = model;
        mView = view;
        PropertyModelChangeProcessor.create(mModel, mView, new SearchBoxViewBinder());

        mTransitionEndOffset =
                !isTablet
                        ? context.getResources()
                                .getDimensionPixelSize(
                                        org.chromium.chrome.R.dimen
                                                .ntp_search_box_transition_end_offset)
                        : 0;
    }

    /**
     * Initializes the SearchBoxContainerView with the given params. This must be called for
     * classes that use the SearchBoxContainerView.
     *
     * @param activityLifecycleDispatcher Used to register for lifecycle events.
     */
    void initialize(ActivityLifecycleDispatcher activityLifecycleDispatcher) {
        assert mActivityLifecycleDispatcher == null;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mActivityLifecycleDispatcher.register(this);
    }

    @Override
    public void onDestroy() {
        if (mActivityLifecycleDispatcher != null) {
            mActivityLifecycleDispatcher.unregister(this);
            mActivityLifecycleDispatcher = null;
        }

        mModel.set(SearchBoxProperties.LENS_CLICK_CALLBACK, null);
        mModel.set(SearchBoxProperties.VOICE_SEARCH_CLICK_CALLBACK, null);
        mModel.set(SearchBoxProperties.VOICE_SEARCH_DRAWABLE, null);
        mModel.set(SearchBoxProperties.SEARCH_BOX_CLICK_CALLBACK, null);
        mModel.set(SearchBoxProperties.SEARCH_BOX_DRAG_CALLBACK, null);
        mModel.set(SearchBoxProperties.SEARCH_BOX_TEXT_WATCHER, null);

        mLensClickListeners.clear();
        mVoiceSearchClickListeners.clear();
    }

    /** Called to set a click listener for the search box. */
    void setSearchBoxClickListener(OnClickListener listener) {
        mModel.set(SearchBoxProperties.SEARCH_BOX_CLICK_CALLBACK, v -> listener.onClick(v));
    }

    /** Called to set a drag listener for the search box. */
    void setSearchBoxDragListener(OnDragListener listener) {
        mModel.set(SearchBoxProperties.SEARCH_BOX_DRAG_CALLBACK, listener);
    }

    /** Called to add a click listener for the voice search button. */
    void addVoiceSearchButtonClickListener(OnClickListener listener) {
        boolean hasExistingListeners = !mVoiceSearchClickListeners.isEmpty();
        mVoiceSearchClickListeners.add(listener);
        if (hasExistingListeners) return;
        mModel.set(
                SearchBoxProperties.VOICE_SEARCH_CLICK_CALLBACK,
                v -> {
                    for (OnClickListener clickListener : mVoiceSearchClickListeners) {
                        clickListener.onClick(v);
                    }
                });
    }

    /** Called to add a click listener for the voice search button. */
    void addLensButtonClickListener(OnClickListener listener) {
        boolean hasExistingListeners = !mLensClickListeners.isEmpty();
        mLensClickListeners.add(listener);
        if (hasExistingListeners) return;
        mModel.set(
                SearchBoxProperties.LENS_CLICK_CALLBACK,
                v -> {
                    for (OnClickListener clickListener : mLensClickListeners) {
                        clickListener.onClick(v);
                    }
                });
    }

    /**
     * Launch the Lens app.
     *
     * @param lensEntryPoint A {@link LensEntryPoint}.
     * @param windowAndroid A {@link WindowAndroid} instance.
     * @param isIncognito Whether the request is from a Incognito tab.
     */
    void startLens(
            @LensEntryPoint int lensEntryPoint, WindowAndroid windowAndroid, boolean isIncognito) {
        LensController.getInstance()
                .startLens(
                        windowAndroid,
                        new LensIntentParams.Builder(lensEntryPoint, isIncognito).build());
    }

    /**
     * Check whether the Lens is enabled for an entry point.
     * @param lensEntryPoint A {@link LensEntryPoint}.
     * @param isIncognito Whether the request is from a Incognito tab.
     * @param isTablet Whether the request is from a tablet.
     * @return Whether the Lens is currently enabled.
     */
    boolean isLensEnabled(
            @LensEntryPoint int lensEntryPoint, boolean isIncognito, boolean isTablet) {
        return LensController.getInstance()
                .isLensEnabled(
                        new LensQueryParams.Builder(lensEntryPoint, isIncognito, isTablet).build());
    }

    void setHeight(int height) {
        mModel.set(SearchBoxProperties.SEARCH_BOX_HEIGHT, height);
    }

    void setTopMargin(int topMargin) {
        mModel.set(SearchBoxProperties.SEARCH_BOX_TOP_MARGIN, topMargin);
    }

    void setEndPadding(int endPadding) {
        mModel.set(SearchBoxProperties.SEARCH_BOX_END_PADDING, endPadding);
    }

    void setStartPadding(int startPadding) {
        mModel.set(SearchBoxProperties.SEARCH_BOX_START_PADDING, startPadding);
    }

    void setSearchBoxTextAppearance(@StyleRes int resId) {
        mModel.set(SearchBoxProperties.SEARCH_BOX_TEXT_STYLE_RES_ID, resId);
    }

    void enableSearchBoxEditText(boolean enabled) {
        mModel.set(SearchBoxProperties.ENABLE_SEARCH_BOX_EDIT_TEXT, enabled);
    }

    void setSearchBoxHintText(@Nullable String hint) {
        mModel.set(SearchBoxProperties.SEARCH_BOX_HINT_TEXT, hint);
    }

    void applyWhiteBackgroundWithShadow(boolean apply) {
        ColorStateList colorStateList =
                ComposeplateUtils.getSearchBoxIconColorTint(mContext, apply);
        mModel.set(SearchBoxProperties.VOICE_SEARCH_COLOR_STATE_LIST, colorStateList);

        mModel.set(SearchBoxProperties.APPLY_WHITE_BACKGROUND_WITH_SHADOW, apply);

        @StyleRes
        int resId =
                apply
                        ? R.style.TextAppearance_FakeSearchBoxTextMediumDark
                        : R.style.TextAppearance_FakeSearchBoxTextMedium;
        mModel.set(SearchBoxProperties.SEARCH_BOX_TEXT_STYLE_RES_ID, resId);
    }

    /**
     * Calculates the percentage (between 0 and 1) of the transition from the search box to the
     * omnibox at the top of the New Tab Page, which is determined by the amount of scrolling and
     * the position of the search box.
     *
     * @return the transition percentage
     */
    float getToolbarTransitionPercentage(
            FeedSurfaceScrollDelegate scrollDelegate,
            @Nullable Supplier<Integer> tabStripHeightSupplier,
            int currentNtpFakeSearchBoxTransitionStartOffset) {
        // During startup the view may not be fully initialized.
        if (!scrollDelegate.isScrollViewInitialized() || mView == null) return 0f;

        if (isSearchBoxOffscreen(scrollDelegate)) {
            // getVerticalScrollOffset is valid only for the scroll view if the first item is
            // visible. If the search box view is offscreen, we must have scrolled quite far and we
            // know the toolbar transition should be 100%. This might be the initial scroll position
            // due to the scroll restore feature, so the search box will not have been laid out yet.
            return 1f;
        }

        // During startup the view may not be fully initialized, so we only calculate the current
        // percentage if some basic view properties (position of the search box) are sane.
        int searchBoxTop = mView.getTop();
        if (searchBoxTop == 0) return 0f;

        // For all other calculations, add the search box padding, because it defines where the
        // visible "border" of the search box is.
        searchBoxTop += mView.getPaddingTop();

        final int scrollY = scrollDelegate.getVerticalScrollOffset();
        // Use int pixel size instead of float dimension to avoid precision error on the percentage.
        final float transitionLength =
                currentNtpFakeSearchBoxTransitionStartOffset + mTransitionEndOffset;
        // Tab strip height is zero on phones, and may vary on tablets.
        int tabStripHeight = tabStripHeightSupplier != null ? tabStripHeightSupplier.get() : 0;

        // When scrollY equals searchBoxTop + tabStripHeight -transitionStartOffset, it marks the
        // start point of the transition. When scrollY equals searchBoxTop plus transitionEndOffset
        // plus tabStripHeight, it marks the end point of the transition.
        return MathUtils.clamp(
                (scrollY
                                - (searchBoxTop + mTransitionEndOffset)
                                + tabStripHeight
                                + transitionLength)
                        / transitionLength,
                0f,
                1f);
    }

    /**
     * Get the bounds of the search box in relation to the top level {@code parentView}.
     *
     * @param bounds The current drawing location of the search box.
     * @param translation The translation applied to the search box by the parent view hierarchy up
     *     to the {@code parentView}.
     * @param parentView The top level parent view used to translate search box bounds.
     * @param scrollDelegate The scroll delegate for NTP.
     * @param searchBoxBoundsVerticalInset Vertical inset to add to the top and bottom of the search
     *     box bounds. May be 0 if no inset should be applied.
     */
    void getSearchBoxBounds(
            Rect bounds,
            Point translation,
            View parentView,
            FeedSurfaceScrollDelegate scrollDelegate,
            int searchBoxBoundsVerticalInset) {
        int searchBoxX = (int) mView.getX();
        int searchBoxY = (int) mView.getY();
        bounds.set(
                searchBoxX,
                searchBoxY,
                searchBoxX + mView.getWidth(),
                searchBoxY + mView.getHeight());

        translation.set(0, 0);

        if (isSearchBoxOffscreen(scrollDelegate)) {
            translation.y = Integer.MIN_VALUE;
        } else {
            View view = mView;
            while (true) {
                view = (View) view.getParent();
                if (view == null) {
                    // The |mView| is not a child of this view. This can happen if the
                    // RecyclerView detaches the NewTabPageLayout after it has been scrolled out of
                    // view. Set the translation to the minimum Y value as an approximation.
                    translation.y = Integer.MIN_VALUE;
                    break;
                }
                translation.offset(-view.getScrollX(), -view.getScrollY());
                if (view == parentView) break;
                translation.offset((int) view.getX(), (int) view.getY());
            }
        }

        bounds.offset(translation.x, translation.y);
        if (translation.y != Integer.MIN_VALUE) {
            bounds.inset(0, searchBoxBoundsVerticalInset);
        }
    }

    /** Returns whether the search box view is scrolled off the screen. */
    boolean isSearchBoxOffscreen(FeedSurfaceScrollDelegate scrollDelegate) {
        if (!scrollDelegate.isScrollViewInitialized()) return false;

        return !scrollDelegate.isChildVisibleAtPosition(0)
                || scrollDelegate.getVerticalScrollOffset() > mView.getTop() + mTransitionEndOffset;
    }
}
