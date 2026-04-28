// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.search;

import android.graphics.Point;
import android.graphics.Rect;
import android.text.TextWatcher;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnDragListener;

import androidx.annotation.Px;
import androidx.annotation.StyleRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.feed.FeedSurfaceScrollDelegate;
import org.chromium.chrome.browser.lens.LensEntryPoint;
import org.chromium.chrome.browser.omnibox.status.StatusProperties.StatusIconResource;

import java.util.function.Supplier;

/** Interface for the search box component on the New Tab Page. */
@NullMarked
public interface NtpSearchBox {
    View getView();

    void destroy();

    void setAlpha(float alpha);

    void setSearchText(String text);

    void setSearchBoxClickListener(OnClickListener listener);

    void setSearchBoxDragListener(OnDragListener listener);

    void setSearchBoxTextWatcher(TextWatcher textWatcher);

    void setVoiceSearchButtonVisibility(boolean visible);

    void addVoiceSearchButtonClickListener(OnClickListener listener);

    void setLensButtonVisibility(boolean visible);

    void addLensButtonClickListener(OnClickListener listener);

    boolean isLensEnabled(@LensEntryPoint int lensEntryPoint);

    void startLens(@LensEntryPoint int lensEntryPoint);

    void setHeight(@Px int height);

    void setTopMargin(@Px int topMargin);

    void setEndPadding(@Px int endPadding);

    void setSearchBoxTextAppearance(@StyleRes int resId);

    void enableSearchBoxEditText(boolean enabled);

    void setSearchBoxHintText(@Nullable String hint);

    void setSearchEngineIcon(@Nullable StatusIconResource icon);

    void applyWhiteBackground(boolean apply);

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
            @Px int currentNtpFakeSearchBoxTransitionStartOffset);

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
            @Px int searchBoxBoundsVerticalInset);

    /**
     * Called in onMeasure() to set the width of the search box. Note: we don't call
     * setLayoutParams() to prevent a second pass of onMeasure().
     *
     * @param widthPx The width of the search box in pixels.
     */
    void setLayoutWidth(@Px int widthPx);
}
