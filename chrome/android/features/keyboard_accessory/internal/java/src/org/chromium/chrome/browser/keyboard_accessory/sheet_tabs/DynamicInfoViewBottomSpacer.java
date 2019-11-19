// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import android.graphics.Rect;
import android.support.v7.widget.RecyclerView;
import android.view.View;

import org.chromium.chrome.browser.keyboard_accessory.R;

/**
 * This decoration adds a space between the last info view and the first non-info view. This allows
 * to define a margin below the whole list of info views without having to wrap them in a new
 * layout. This would reduce the reusability of single info containers in a RecyclerView.
 */
class DynamicInfoViewBottomSpacer extends RecyclerView.ItemDecoration {
    private final Class mInfoViewClass;

    DynamicInfoViewBottomSpacer(Class infoViewClass) {
        mInfoViewClass = infoViewClass;
    }

    @Override
    public void getItemOffsets(
            Rect outRect, View view, RecyclerView parent, RecyclerView.State state) {
        super.getItemOffsets(outRect, view, parent, state);
        if (isUserInfoView(view)) return;
        int previous = parent.indexOfChild(view) - 1;
        if (previous < 0) return;
        if (!isUserInfoView(parent.getChildAt(previous))) return;
        outRect.top = view.getContext().getResources().getDimensionPixelSize(
                R.dimen.keyboard_accessory_suggestion_padding);
    }

    private boolean isUserInfoView(View view) {
        return view.getClass().getCanonicalName().equals(mInfoViewClass.getCanonicalName());
    }
}
