// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.content.Context;
import android.util.AttributeSet;
import android.view.Gravity;
import android.widget.LinearLayout;
import android.widget.ScrollView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.widget.ButtonCompat;

/** View that handles orientation changes for Sad Tab / Crashed Renderer page. */
@NullMarked
public class SadTabView extends ScrollView {
    // Dimension (dp) at which reload button is dynamically sized and content centers
    private static final int MAX_BUTTON_WIDTH_DP = 620;
    private final int mThresholdPx;

    public SadTabView(Context context, AttributeSet attrs) {
        super(context, attrs);
        float density = context.getResources().getDisplayMetrics().density;
        mThresholdPx = (int) (density * MAX_BUTTON_WIDTH_DP);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        // This assumes that view's layout_width is set to match_parent.
        assert MeasureSpec.getMode(widthMeasureSpec) == MeasureSpec.EXACTLY;
        int width = MeasureSpec.getSize(widthMeasureSpec);
        int height = MeasureSpec.getSize(heightMeasureSpec);

        final ButtonCompat button = (ButtonCompat) findViewById(R.id.sad_tab_button);

        final LinearLayout.LayoutParams buttonParams =
                (LinearLayout.LayoutParams) button.getLayoutParams();

        if ((width > height || width > mThresholdPx) && button.getWidth() <= width) {
            // Orientation is landscape
            buttonParams.width = LinearLayout.LayoutParams.WRAP_CONTENT;
            buttonParams.gravity = Gravity.END;
        } else {
            // Orientation is portrait
            buttonParams.width = LinearLayout.LayoutParams.MATCH_PARENT;
            buttonParams.gravity = Gravity.FILL_HORIZONTAL;
        }

        button.setLayoutParams(buttonParams);
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }
}
