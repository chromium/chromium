// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_resumption;

import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.widget.LinearLayout;

import androidx.annotation.Nullable;

/**
 * The view for the entire search resumption layout, including a header and the section of a set of
 * search suggestions.
 */
public class SearchResumptionContainerView extends LinearLayout {
    public SearchResumptionContainerView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Inflates a {@link SearchResumptionTileView} instance.
     */
    SearchResumptionTileView buildTileView() {
        return (SearchResumptionTileView) LayoutInflater.from(getContext())
                .inflate(R.layout.search_resumption_module_tile_layout, this, false);
    }

    void destroy() {
        for (int i = 0; i < getChildCount(); i++) {
            ((SearchResumptionTileView) getChildAt(i)).destroy();
        }
        removeAllViews();
    }
}
