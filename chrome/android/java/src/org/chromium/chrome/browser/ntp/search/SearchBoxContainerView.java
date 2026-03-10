// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.search;

import android.content.Context;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.RippleDrawable;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.core.widget.ImageViewCompat;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.composeplate.ComposeplateUtils;
import org.chromium.components.browser_ui.widget.RoundedCornerOutlineProvider;

/** Provides the additional capabilities needed for the SearchBox container layout. */
@NullMarked
public class SearchBoxContainerView extends LinearLayout {
    private static final String TAG = "SearchBoxContainer";
    private final int mPaddingForShadowLateralPx;
    private final int mPaddingForShadowBottomPx;

    private ImageView mDseIconView;

    /** Constructor for inflating from XML. */
    public SearchBoxContainerView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mPaddingForShadowLateralPx =
                context.getResources()
                        .getDimensionPixelSize(
                                R.dimen.composeplate_view_button_padding_for_shadow_lateral);
        mPaddingForShadowBottomPx =
                context.getResources()
                        .getDimensionPixelSize(
                                R.dimen.composeplate_view_button_padding_for_shadow_bottom);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        // TODO(crbug.com/347509698): Remove the log statements after fixing the bug.
        Log.i(TAG, "SearchBoxContainerView.onFinishInflate before set typeface");

        TextView searchBoxTextView = findViewById(R.id.search_box_text);
        Typeface typeface = Typeface.create("google-sans-medium", Typeface.NORMAL);
        searchBoxTextView.setTypeface(typeface);

        mDseIconView = findViewById(R.id.search_box_engine_icon);
        mDseIconView.setOutlineProvider(
                new RoundedCornerOutlineProvider(
                        getResources()
                                        .getDimensionPixelSize(
                                                R.dimen.omnibox_search_engine_logo_composed_size)
                                / 2));
        mDseIconView.setClipToOutline(true);
        ImageViewCompat.setImageTintList(mDseIconView, null);

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

    void setDseIconResource(@DrawableRes int resId) {
        mDseIconView.setImageResource(resId);
    }

    void setDseIconDrawable(@Nullable Drawable drawable) {
        mDseIconView.setImageDrawable(drawable);
    }

    /**
     * Applies or cleans up the white background for the search box.
     *
     * @param apply Whether to apply a white background color to the fake search box.
     */
    void applyWhiteBackgroundWithShadow(boolean apply) {
        Context context = getContext();
        if (apply) {
            // Adds paddings on each sides of the view to prevent shadow from being cut.
            setPadding(
                    mPaddingForShadowLateralPx,
                    mPaddingForShadowBottomPx,
                    mPaddingForShadowLateralPx,
                    mPaddingForShadowBottomPx);
        } else {
            setPadding(0, 0, 0, 0);
        }

        View searchBoxContainerView = findViewById(R.id.search_box_container);
        if (searchBoxContainerView != null) {
            ComposeplateUtils.applyWhiteBackgroundAndShadow(context, searchBoxContainerView, apply);
        }
    }
}
