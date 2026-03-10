// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.search;

import android.content.Context;
import android.graphics.Point;
import android.graphics.Rect;
import android.text.TextWatcher;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnDragListener;
import android.view.ViewGroup;

import androidx.annotation.StyleRes;

import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feed.FeedSurfaceScrollDelegate;
import org.chromium.chrome.browser.lens.LensEntryPoint;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.function.Supplier;

/**
 * This class is responsible for reacting to events from the outside world, interacting with other
 * coordinators, running most of the business logic associated with the fake search box component,
 * and updating the model accordingly.
 */
@NullMarked
public class SearchBoxCoordinator {
    private final PropertyModel mModel;
    private final ViewGroup mView;
    private final SearchBoxMediator mMediator;
    private boolean mIsIncognito;
    private WindowAndroid mWindowAndroid;

    /** Constructor. */
    public SearchBoxCoordinator(Context context, ViewGroup parent, boolean isTablet) {
        mModel = new PropertyModel(SearchBoxProperties.ALL_KEYS);
        mView = parent.findViewById(R.id.search_box);
        mMediator = new SearchBoxMediator(context, mModel, mView, isTablet);
    }

    @Initializer
    public void initialize(
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            boolean isIncognito,
            WindowAndroid windowAndroid) {
        mMediator.initialize(activityLifecycleDispatcher);
        mIsIncognito = isIncognito;
        mWindowAndroid = windowAndroid;
    }

    public View getView() {
        return mView;
    }

    public void destroy() {
        mMediator.onDestroy();
    }

    public void setAlpha(float alpha) {
        mModel.set(SearchBoxProperties.ALPHA, alpha);
    }

    public void setSearchText(String text) {
        mModel.set(SearchBoxProperties.SEARCH_TEXT, text);
    }

    public void setSearchBoxClickListener(OnClickListener listener) {
        mMediator.setSearchBoxClickListener(listener);
    }

    public void setSearchBoxDragListener(OnDragListener listener) {
        mMediator.setSearchBoxDragListener(listener);
    }

    public void setSearchBoxTextWatcher(TextWatcher textWatcher) {
        mModel.set(SearchBoxProperties.SEARCH_BOX_TEXT_WATCHER, textWatcher);
    }

    public void setVoiceSearchButtonVisibility(boolean visible) {
        mModel.set(SearchBoxProperties.VOICE_SEARCH_VISIBILITY, visible);
    }

    public void addVoiceSearchButtonClickListener(OnClickListener listener) {
        mMediator.addVoiceSearchButtonClickListener(listener);
    }

    public void setLensButtonVisibility(boolean visible) {
        mModel.set(SearchBoxProperties.LENS_VISIBILITY, visible);
    }

    public void addLensButtonClickListener(OnClickListener listener) {
        mMediator.addLensButtonClickListener(listener);
    }

    public boolean isLensEnabled(@LensEntryPoint int lensEntryPoint) {
        return mMediator.isLensEnabled(
                lensEntryPoint, mIsIncognito, DeviceFormFactor.isWindowOnTablet(mWindowAndroid));
    }

    public void startLens(@LensEntryPoint int lensEntryPoint) {
        mMediator.startLens(lensEntryPoint, mWindowAndroid, mIsIncognito);
    }

    public void setHeight(int height) {
        mMediator.setHeight(height);
    }

    public void setTopMargin(int topMargin) {
        mMediator.setTopMargin(topMargin);
    }

    public void setEndPadding(int endPadding) {
        mMediator.setEndPadding(endPadding);
    }

    public void setStartPadding(int startPadding) {
        mMediator.setStartPadding(startPadding);
    }

    public void setSearchBoxTextAppearance(@StyleRes int resId) {
        mMediator.setSearchBoxTextAppearance(resId);
    }

    public void enableSearchBoxEditText(boolean enabled) {
        mMediator.enableSearchBoxEditText(enabled);
    }

    public void setSearchBoxHintText(@Nullable String hint) {
        mMediator.setSearchBoxHintText(hint);
    }

    public void applyWhiteBackgroundWithShadow(boolean apply) {
        mMediator.applyWhiteBackgroundWithShadow(apply);
    }

    /**
     * Calculates the percentage (between 0 and 1) of the transition from the search box to the
     * omnibox at the top of the New Tab Page, which is determined by the amount of scrolling and
     * the position of the search box.
     *
     * @return the transition percentage
     */
    public float getToolbarTransitionPercentage(
            FeedSurfaceScrollDelegate scrollDelegate,
            @Nullable Supplier<Integer> tabStripHeightSupplier,
            int currentNtpFakeSearchBoxTransitionStartOffset) {
        return mMediator.getToolbarTransitionPercentage(
                scrollDelegate,
                tabStripHeightSupplier,
                currentNtpFakeSearchBoxTransitionStartOffset);
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
    public void getSearchBoxBounds(
            Rect bounds,
            Point translation,
            View parentView,
            FeedSurfaceScrollDelegate scrollDelegate,
            int searchBoxBoundsVerticalInset) {
        mMediator.getSearchBoxBounds(
                bounds, translation, parentView, scrollDelegate, searchBoxBoundsVerticalInset);
    }

    /**
     * Called in onMeasure() to set the width of the search box. Note: we don't call
     * setLayoutParams() to prevent a second pass of onMeasure().
     *
     * @param widthPx The width of the search box in pixels.
     */
    public void setLayoutWidth(int widthPx) {
        ViewGroup.MarginLayoutParams marginLayoutParams =
                (ViewGroup.MarginLayoutParams) mView.getLayoutParams();
        if (marginLayoutParams.width != widthPx
                || marginLayoutParams.leftMargin != 0
                || marginLayoutParams.rightMargin != 0) {
            marginLayoutParams.width = widthPx;
            marginLayoutParams.leftMargin = 0;
            marginLayoutParams.rightMargin = 0;
        }
    }
}
