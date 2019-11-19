// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility_tab_switcher;

import android.content.Context;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.widget.ListView;

/**
 * A {@link ListView} class that is responsible for providing a visual interpretation
 * of a {@link org.chromium.chrome.browser.tabmodel.TabModel}.
 */
public class AccessibilityTabModelListView extends ListView {
    private final AccessibilityTabModelAdapter mAdapter;
    private boolean mCanScrollVertically = true;

    /**
     * @param context The Context to build this widget under.
     * @param attrs The AttributeSet to use to build this widget.
     */
    public AccessibilityTabModelListView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mAdapter = new AccessibilityTabModelAdapter(getContext(), this);
    }

    @Override
    public void onFinishInflate() {
        super.onFinishInflate();

        setAdapter(mAdapter);
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent e) {
        // Ignore touch events if we're not scrolling.
        if (!mCanScrollVertically) return false;
        return super.onInterceptTouchEvent(e);
    }

    /**
     * @param canScroll Whether or not the ListView should be allowed to scroll vertically.
     */
    public void setCanScroll(boolean canScroll) {
        mCanScrollVertically = canScroll;
    }
}
