// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.carousel;

import android.content.Context;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionProcessor;

/** The base processor implementation for the Carousel suggestions. */
public abstract class BaseCarouselSuggestionProcessor implements SuggestionProcessor {
    private final int mCarouselViewDecorationHeightPx;

    /**
     * @param context Current context.
     */
    public BaseCarouselSuggestionProcessor(Context context) {
        mCarouselViewDecorationHeightPx = context.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_header_height);
    }

    /**
     * @return The height of the Carousel view for use when computing view minimum height.
     */
    @Override
    public final int getMinimumViewHeight() {
        return mCarouselViewDecorationHeightPx + getMinimumCarouselItemViewHeight();
    }

    /**
     * @return Minimum height of an element hosted by the carousel.
     */
    public abstract int getMinimumCarouselItemViewHeight();
}
