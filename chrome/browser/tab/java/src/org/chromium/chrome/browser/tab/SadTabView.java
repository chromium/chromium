// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.content.Context;
import android.util.AttributeSet;
import android.view.Gravity;
import android.widget.LinearLayout;
import android.widget.ScrollView;

import org.chromium.ui.widget.ButtonCompat;

/** View that handles orientation changes for Sad Tab / Crashed Renderer page. */
public class SadTabView extends ScrollView {
    // Dimension (dp) at which reload button is dynamically sized and content centers
    private static final int MAX_BUTTON_WIDTH_DP = 620;
    private int mThresholdPx;
    private float mDensity;

    public SadTabView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mDensity = context.getResources().getDisplayMetrics().density;
        mThresholdPx = (int) (mDensity * MAX_BUTTON_WIDTH_DP);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        // This assumes that view's layout_width is set to match_parent.
        assert MeasureSpec.getMode(widthMeasureSpec) == MeasureSpec.EXACTLY;
        int width = MeasureSpec.getSize(widthMeasureSpec);
        int height = MeasureSpec.getSize(heightMeasureSpec);

        final ButtonCompat mButton = (ButtonCompat) findViewById(R.id.sad_tab_button);

        final LinearLayout.LayoutParams mButtonParams =
                (LinearLayout.LayoutParams) mButton.getLayoutParams();

        if ((width > height || width > mThresholdPx) && mButton.getWidth() <= width) {
            // Orientation is landscape
            mButtonParams.width = LinearLayout.LayoutParams.WRAP_CONTENT;
            mButtonParams.gravity = Gravity.END;
        } else {
            // Orientation is portrait
            mButtonParams.width = LinearLayout.LayoutParams.MATCH_PARENT;
            mButtonParams.gravity = Gravity.FILL_HORIZONTAL;
        }

        mButton.setLayoutParams(mButtonParams);
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }
}
