// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.sections;

import static android.view.View.GONE;
import static android.view.View.VISIBLE;

import android.content.Context;
import android.util.AttributeSet;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.annotation.Px;

/**
 * View for the feed header that sticks to the top of the screen upon scroll.
 */
public class StickySectionHeaderView extends SectionHeaderView {
    public StickySectionHeaderView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    /** Sets the sticky header’s visibility. */
    @Override
    void setStickyHeaderVisible(boolean isVisible) {
        this.setVisibility(isVisible ? VISIBLE : GONE);
    }

    /**
     * When we set/update the toolbar height, the margin of the sticky header should be updated
     * simultaneously.
     */
    @Override
    public void setToolbarHeight(@Px int toolbarHeight) {
        super.setToolbarHeight(toolbarHeight);
        ViewGroup.MarginLayoutParams params = (ViewGroup.MarginLayoutParams) this.getLayoutParams();
        params.setMargins(
                params.leftMargin, toolbarHeight, params.rightMargin, params.bottomMargin);
    }
}
