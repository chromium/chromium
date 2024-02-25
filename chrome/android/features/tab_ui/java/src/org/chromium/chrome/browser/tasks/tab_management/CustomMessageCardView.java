// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.LinearLayout;

/**
 * Represents an empty custom message card view in the Grid Tab Switcher. This view supports
 * attaching a custom message card design to an empty message card view and displaying it. This view
 * is not responsible for handling the attached child view's model and subsequent functionality.
 */
public class CustomMessageCardView extends LinearLayout {
    public CustomMessageCardView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
    }

    /**
     * Sets the custom view to be displayed as a child view of this message card view.
     *
     * @param view The view to be displayed.
     */
    public void setChildView(View view) {
        addView(
                view,
                new LinearLayout.LayoutParams(
                        LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
    }
}
