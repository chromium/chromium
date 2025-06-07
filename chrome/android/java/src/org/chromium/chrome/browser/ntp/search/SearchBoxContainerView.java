// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.search;


import android.content.Context;
import android.graphics.Typeface;
import android.graphics.drawable.RippleDrawable;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;

/** Provides the additional capabilities needed for the SearchBox container layout. */
@NullMarked
public class SearchBoxContainerView extends LinearLayout {
    private static final String TAG = "SearchBoxContainer";
    private View mComposeplateButtonView;

    /** Constructor for inflating from XML. */
    public SearchBoxContainerView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        // TODO(crbug.com/347509698): Remove the log statements after fixing the bug.
        Log.i(TAG, "SearchBoxContainerView.onFinishInflate before set typeface");

        TextView searchBoxTextView = findViewById(R.id.search_box_text);
        Typeface typeface = Typeface.create("google-sans-medium", Typeface.NORMAL);
        searchBoxTextView.setTypeface(typeface);

        mComposeplateButtonView = findViewById(R.id.composeplate_button);

        Log.i(TAG, "SearchBoxContainerView.onFinishInflate after set typeface");
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent ev) {
        if (ev.getActionMasked() == MotionEvent.ACTION_DOWN) {
            if (getBackground() instanceof RippleDrawable) {
                ((RippleDrawable) getBackground()).setHotspot(ev.getX(), ev.getY());
            }
        }
        return super.onInterceptTouchEvent(ev);
    }

    void setComposeplateButtonVisibility(boolean isVisible) {
        mComposeplateButtonView.setVisibility(isVisible ? View.VISIBLE : View.GONE);
        int endPaddingInDp =
                isVisible
                        ? R.dimen.fake_search_box_with_composeplate_button_end_padding
                        : R.dimen.fake_search_box_end_padding;
        setPaddingRelative(
                getPaddingStart(),
                getPaddingTop(),
                getResources().getDimensionPixelSize(endPaddingInDp),
                getPaddingBottom());
    }
}
