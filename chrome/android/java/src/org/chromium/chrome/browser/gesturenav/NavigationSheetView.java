// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ListView;
import android.widget.RelativeLayout;

import org.chromium.chrome.R;

/**
 * {@link View} class for navigation sheet. Provided as content view for
 * {@link BottomSheet}.
 */
public class NavigationSheetView extends RelativeLayout {
    private ListView mListView;

    public NavigationSheetView(Context context) {
        this(context, null);
    }

    public NavigationSheetView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * @return The vertical scroll offset of the list view containing the navigation history items.
     */
    int getVerticalScrollOffset() {
        View v = mListView.getChildAt(0);
        return v == null ? 0 : -(v.getTop() - mListView.getPaddingTop());
    }

    @Override
    public void onFinishInflate() {
        super.onFinishInflate();
        mListView = findViewById(R.id.navigation_entries);
    }
}
