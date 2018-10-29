// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.displaystyle;

import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;

/**
 * Changes a view's margins when switching between {@link UiConfig} display styles.
 */
public class MarginResizer implements DisplayStyleObserver {
    /** The default value for the lateral margins. */
    private int mDefaultMarginSizePixels;
    /** The wide display value for the lateral margins. */
    private int mWideMarginSizePixels;
    private final View mView;
    private final DisplayStyleObserverAdapter mDisplayStyleObserver;

    @HorizontalDisplayStyle
    private int mCurrentDisplayStyle;

    /**
     * @param view The view that will have its margins resized.
     * @param config The UiConfig object to subscribe to.
     * @param defaultMarginPixels Margin size to use in {@link HorizontalDisplayStyle#REGULAR}.
     * @param wideMarginPixels Margin size to use in {@link HorizontalDisplayStyle#WIDE}.
     */
    public MarginResizer(
            View view, UiConfig config, int defaultMarginPixels, int wideMarginPixels) {
        mView = view;
        mDefaultMarginSizePixels = defaultMarginPixels;
        mWideMarginSizePixels = wideMarginPixels;
        mDisplayStyleObserver = new DisplayStyleObserverAdapter(view, config, this);
    }

    /**
     * Convenience method to create a new MarginResizer and immediately attach it to a {@link
     * UiConfig}. If the {@link UiConfig} can outlive the view, the regular constructor should be
     * used, so it can be detached to avoid memory leaks.
     * @param view The view that will have its margins resized.
     * @param config The UiConfig object to subscribe to.
     * @param defaultMarginPixels Margin size to use in {@link HorizontalDisplayStyle#REGULAR}.
     * @param wideMarginPixels Margin size to use in {@link HorizontalDisplayStyle#WIDE}.
     * @return The {@link MarginResizer} that is created and attached.
     */
    public static MarginResizer createAndAttach(
            View view, UiConfig config, int defaultMarginPixels, int wideMarginPixels) {
        MarginResizer marginResizer =
                new MarginResizer(view, config, defaultMarginPixels, wideMarginPixels);
        marginResizer.attach();
        return marginResizer;
    }

    /**
     * Attaches to the {@link UiConfig}.
     */
    public void attach() {
        mDisplayStyleObserver.attach();
    }

    /**
     * Detaches from the {@link UiConfig}.
     */
    public void detach() {
        mDisplayStyleObserver.detach();
    }

    @Override
    public void onDisplayStyleChanged(UiConfig.DisplayStyle newDisplayStyle) {
        mCurrentDisplayStyle = newDisplayStyle.horizontal;
        updateMargins();
    }

    /**
     * Sets the lateral margins on the associated view, using the appropriate value depending on
     * the current display style.
     * @param defaultMarginPixels Margin size to use in {@link HorizontalDisplayStyle#REGULAR}.
     * @param wideMarginPixels Margin size to use in {@link HorizontalDisplayStyle#WIDE}.
     */
    public void setMargins(int defaultMarginPixels, int wideMarginPixels) {
        mDefaultMarginSizePixels = defaultMarginPixels;
        mWideMarginSizePixels = wideMarginPixels;
        updateMargins();
    }

    private void updateMargins() {
        MarginLayoutParams layoutParams = (MarginLayoutParams) mView.getLayoutParams();
        if (mCurrentDisplayStyle == HorizontalDisplayStyle.WIDE) {
            layoutParams.setMargins(mWideMarginSizePixels, layoutParams.topMargin,
                    mWideMarginSizePixels, layoutParams.bottomMargin);
        } else {
            layoutParams.setMargins(mDefaultMarginSizePixels, layoutParams.topMargin,
                    mDefaultMarginSizePixels, layoutParams.bottomMargin);
        }
        mView.setLayoutParams(layoutParams);
    }
}
