// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ScrollView;

import org.chromium.chrome.R;

/**
 * View that is used when asking the user to choose between options during First Run.
 *
 * Manages the appearance of a large header at the top of the dialog.
 */
public class FirstRunChooserView extends ScrollView {
    private View mChooserTitleView;

    public FirstRunChooserView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mChooserTitleView = findViewById(R.id.chooser_title);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        // This assumes that view's layout_width and layout_height are set to match_parent.
        assert MeasureSpec.getMode(widthMeasureSpec) == MeasureSpec.EXACTLY;
        assert MeasureSpec.getMode(heightMeasureSpec) == MeasureSpec.EXACTLY;

        int width = MeasureSpec.getSize(widthMeasureSpec);
        int height = MeasureSpec.getSize(heightMeasureSpec);

        ViewGroup.LayoutParams params = mChooserTitleView.getLayoutParams();
        if (height > width) {
            // Sets the title aspect ratio to be 16:9.
            params.height = width * 9 / 16;
            mChooserTitleView.setPadding(
                    mChooserTitleView.getPaddingLeft(),
                    0,
                    mChooserTitleView.getPaddingRight(),
                    mChooserTitleView.getPaddingBottom());
        } else {
            params.height = ViewGroup.LayoutParams.WRAP_CONTENT;

            // Adds top padding.
            mChooserTitleView.setPadding(
                    mChooserTitleView.getPaddingLeft(),
                    getResources().getDimensionPixelOffset(R.dimen.signin_screen_top_padding),
                    mChooserTitleView.getPaddingRight(),
                    mChooserTitleView.getPaddingBottom());
        }
        mChooserTitleView.setLayoutParams(params);

        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    @Override
    protected float getTopFadingEdgeStrength() {
        // Disable fading out effect at the top of this ScrollView.
        return 0;
    }
}
