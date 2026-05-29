// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.search;

import android.graphics.Point;
import android.graphics.Rect;
import android.text.TextWatcher;
import android.view.View;
import android.view.View.OnDragListener;
import android.view.ViewStub;

import androidx.annotation.IdRes;
import androidx.annotation.Px;
import androidx.annotation.StyleRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.feed.FeedSurfaceScrollDelegate;
import org.chromium.chrome.browser.lens.LensEntryPoint;
import org.chromium.chrome.browser.omnibox.LocationBarEmbedder;
import org.chromium.chrome.browser.omnibox.status.StatusProperties.StatusIconResource;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.ui.AsyncViewStub;
import org.chromium.ui.base.ViewUtils;

import java.util.function.Supplier;

/** Coordinator for the realbox component on the new tab page, embeds an actual location bar. */
@NullMarked
public class RealboxCoordinator implements NtpSearchBox, LocationBarEmbedder {
    private final View mView;

    /**
     * @param viewStub The {@link ViewStub} to be replaced with the inflated realbox layout.
     * @param backPressManager Used to consume/handle back presses during interactions.
     */
    public RealboxCoordinator(ViewStub viewStub, BackPressManager backPressManager) {
        mView = ViewUtils.replace(viewStub, R.layout.realbox_layout);
    }

    @Override
    public View getView() {
        return mView;
    }

    @Override
    public void destroy() {
        // TODO(https://crbug.com/507131334): Implement.
    }

    @Override
    public void setAlpha(float alpha) {
        // TODO(https://crbug.com/507131334): Implement.
    }

    @Override
    public void setSearchText(String text) {
        // Handled by the location bar.
    }

    @Override
    public void setSearchBoxDragListener(OnDragListener listener) {
        // TODO(https://crbug.com/507131334): Implement.
    }

    @Override
    public void setSearchBoxTextWatcher(TextWatcher textWatcher) {
        // Handled by the location bar.
    }

    @Override
    public void setVoiceSearchButtonVisibility(boolean visible) {
        // Handled by the location bar.
    }

    @Override
    public void setLensButtonVisibility(boolean visible) {
        // Handled by the location bar.
    }

    @Override
    public boolean isLensEnabled(@LensEntryPoint int lensEntryPoint) {
        // TODO(https://crbug.com/507131334): Implement.
        return false;
    }

    @Override
    public void setHeight(@Px int height) {
        // TODO(https://crbug.com/507131334): Implement.
    }

    @Override
    public void setTopMargin(@Px int topMargin) {
        // TODO(https://crbug.com/507131334): Implement.
    }

    @Override
    public void setEndPadding(@Px int endPadding) {
        // TODO(https://crbug.com/507131334): Implement.
    }

    @Override
    public void setSearchBoxTextAppearance(@StyleRes int resId) {
        // Handled by the location bar.
    }

    @Override
    public void enableSearchBoxEditText(boolean enabled) {
        // Handled by the location bar.
    }

    @Override
    public void setSearchBoxHintText(@Nullable String hint) {
        // Handled by the location bar.
    }

    @Override
    public void setSearchEngineIcon(@Nullable StatusIconResource icon) {
        // Handled by the location bar.
    }

    @Override
    public void applyWhiteBackground(boolean apply) {
        // Handled by the location bar.
    }

    @Override
    public float getToolbarTransitionPercentage(
            FeedSurfaceScrollDelegate scrollDelegate,
            @Nullable Supplier<Integer> tabStripHeightSupplier,
            @Px int currentNtpFakeSearchBoxTransitionStartOffset) {
        // The realbox does not transition off.
        return 0f;
    }

    @Override
    public void getSearchBoxBounds(
            Rect bounds,
            Point translation,
            View parentView,
            FeedSurfaceScrollDelegate scrollDelegate,
            @Px int searchBoxBoundsVerticalInset) {
        // Handled by the location bar.
    }

    @Override
    public void setLayoutWidth(@Px int widthPx) {
        // TODO(https://crbug.com/507131334): Implement.
    }

    @Override
    public @Nullable AsyncViewStub getSuggestionsContainerStub() {
        return mView.getRootView().findViewById(R.id.ntp_realbox_results_container_stub);
    }

    @Override
    public @IdRes int getSuggestionsContainerInflatedViewId() {
        return R.id.ntp_realbox_results_container;
    }

    @Override
    public @BackPressHandler.Type int getBackPressHandlerType() {
        return BackPressHandler.Type.REALBOX;
    }
}
