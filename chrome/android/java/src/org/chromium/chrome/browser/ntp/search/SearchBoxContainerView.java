// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.search;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.RippleDrawable;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Px;
import androidx.core.widget.ImageViewCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.composeplate.ComposeplateUtils;
import org.chromium.components.browser_ui.widget.RoundedCornerOutlineProvider;

/** Provides the additional capabilities needed for the SearchBox container layout. */
@NullMarked
public class SearchBoxContainerView extends LinearLayout {
    TextView mHintTextView;
    ImageView mDseIconView;
    ImageView mVoiceSearchButton;
    ImageView mLensButton;
    ImageView mPlusButton;

    /** Constructor for inflating from XML. */
    public SearchBoxContainerView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mHintTextView = findViewById(R.id.search_box_text);
        mDseIconView = findViewById(R.id.search_box_engine_icon);
        mVoiceSearchButton = findViewById(R.id.voice_search_button);
        mLensButton = findViewById(R.id.lens_camera_button);
        mPlusButton = findViewById(R.id.search_box_plus_button);

        Typeface typeface = Typeface.create("google-sans-medium", Typeface.NORMAL);
        mHintTextView.setTypeface(typeface);
        Resources res = getResources();
        @Px int size = res.getDimensionPixelSize(R.dimen.omnibox_search_engine_logo_composed_size);
        @Px int radius = size / 2;
        mDseIconView.setOutlineProvider(new RoundedCornerOutlineProvider(radius));
        mDseIconView.setClipToOutline(true);
        ImageViewCompat.setImageTintList(mDseIconView, /* tintList= */ null);
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent ev) {
        if (ev.getActionMasked() == MotionEvent.ACTION_DOWN) {
            if (getBackground() instanceof RippleDrawable) {
                getBackground().setHotspot(ev.getX(), ev.getY());
            }
        }
        return super.onInterceptTouchEvent(ev);
    }

    void setDseIconDrawable(@Nullable Drawable drawable) {
        mDseIconView.setImageDrawable(drawable);
    }

    void setPlusButtonClickListener(@Nullable OnClickListener listener) {
        mPlusButton.setOnClickListener(listener);
    }

    /**
     * Applies or cleans up the white background for the search box.
     *
     * @param apply Whether to apply a white background color to the fake search box.
     */
    void applyWhiteBackground(boolean apply) {
        ComposeplateUtils.applyWhiteBackground(getContext(), this, apply);
    }
}
