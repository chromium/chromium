// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.search;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Rect;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.LayerDrawable;
import android.graphics.drawable.RippleDrawable;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.TouchDelegate;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Px;
import androidx.core.widget.ImageViewCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.composeplate.ComposeplateUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.omnibox.GlifStrokeDrawable;
import org.chromium.components.browser_ui.widget.RoundedCornerOutlineProvider;
import org.chromium.ui.widget.ButtonCompat;

/** Provides the additional capabilities needed for the SearchBox container layout. */
@NullMarked
public class SearchBoxContainerView extends LinearLayout {
    private final int mPaddingForShadowLateralPx;
    TextView mHintTextView;
    ImageView mDseIconView;
    View mSearchBoxView;
    ImageView mVoiceSearchButton;
    ImageView mLensButton;
    ImageView mPlusButton;
    ButtonCompat mAiChip;
    GlifStrokeDrawable mGlifStrokeDrawable;
    boolean mIsNtpAuroraEnabled;

    private @Nullable TouchDelegate mTouchDelegate;
    private @Nullable Rect mLastTouchDelegateRect;

    /** Constructor for inflating from XML. */
    public SearchBoxContainerView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mPaddingForShadowLateralPx =
                getResources().getDimensionPixelSize(R.dimen.search_box_padding_for_shadow_lateral);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mHintTextView = findViewById(R.id.search_box_text);
        mDseIconView = findViewById(R.id.search_box_engine_icon);
        mSearchBoxView = findViewById(R.id.search_box_shadow_container);
        mVoiceSearchButton = findViewById(R.id.voice_search_button);
        mLensButton = findViewById(R.id.lens_camera_button);
        mPlusButton = findViewById(R.id.search_box_plus_button);
        mAiChip = findViewById(R.id.search_box_ai_chip);
        mPlusButton.addOnLayoutChangeListener(
                (v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {
                    updateTouchDelegate();
                });
        mIsNtpAuroraEnabled = ChromeFeatureList.sNtpAurora.isEnabled();

        Typeface typeface = Typeface.create("google-sans-medium", Typeface.NORMAL);
        mHintTextView.setTypeface(typeface);
        Resources res = getResources();
        @Px int size = res.getDimensionPixelSize(R.dimen.omnibox_search_engine_logo_composed_size);
        @Px int radius = size / 2;
        mDseIconView.setOutlineProvider(new RoundedCornerOutlineProvider(radius));
        mDseIconView.setClipToOutline(true);
        ImageViewCompat.setImageTintList(mDseIconView, /* tintList= */ null);
        float cornerRadius = res.getDimension(R.dimen.ai_chip_corner_radius);
        mGlifStrokeDrawable = new GlifStrokeDrawable(getContext(), cornerRadius);

        LayerDrawable foreground = (LayerDrawable) mAiChip.getForeground();
        foreground.setDrawableByLayerId(R.id.glif_border_layer, mGlifStrokeDrawable);
        mAiChip.setOnHoverListener(
                (v, event) -> {
                    if (event.getAction() == MotionEvent.ACTION_HOVER_ENTER) {
                        mGlifStrokeDrawable.start();
                    }
                    return false;
                });
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
     * Updates the visibility of the plus button and DSE icon, synchronizing the touch delegate.
     *
     * @param visible Whether the plus button should be visible.
     */
    void updateStartIconVisibility(boolean visible) {
        mPlusButton.setVisibility(visible ? View.VISIBLE : View.GONE);
        mDseIconView.setVisibility(visible ? View.GONE : View.VISIBLE);
        updateTouchDelegate();
    }

    /**
     * Applies or cleans up the white background for the search box.
     *
     * @param applyWhiteBackground Whether to apply a white background color to the fake search box.
     */
    void applyWhiteBackgroundAndShadow(boolean applyWhiteBackground) {
        View searchBoxShadowContainerView = findViewById(R.id.search_box_shadow_container);
        if (searchBoxShadowContainerView == null) return;

        ComposeplateUtils.applyWhiteBackground(
                getContext(), searchBoxShadowContainerView, applyWhiteBackground);
        NtpCustomizationUtils.applyShadow(
                getContext(), searchBoxShadowContainerView, mIsNtpAuroraEnabled);
        updateSearchBoxPaddingAndMarginForShadow(mIsNtpAuroraEnabled);
    }

    private void updateSearchBoxPaddingAndMarginForShadow(boolean applyShadow) {
        ViewGroup.MarginLayoutParams layoutParams =
                (ViewGroup.MarginLayoutParams) getLayoutParams();
        if (layoutParams == null) return;

        if (applyShadow) {
            setPadding(
                    mPaddingForShadowLateralPx,
                    getPaddingTop(),
                    mPaddingForShadowLateralPx,
                    getPaddingBottom());
        } else {
            setPadding(0, getPaddingTop(), 0, getPaddingBottom());
        }

        setLayoutParams(layoutParams);
    }

    /**
     * Applies elevation to the search box.
     *
     * @param apply Whether to apply elevation to the search box.
     */
    void applyElevation(boolean apply) {
        if (!apply) {
            mSearchBoxView.setElevation(0);
            // Reset clipping to default to avoid unexpected behavior.
            setClipToPadding(true);
            setClipChildren(true);
            return;
        }

        float elevation = getResources().getDimension(R.dimen.fake_search_box_elevation);
        mSearchBoxView.setElevation(elevation);

        // Disable clipping to allow the shadow to be drawn outside the view bounds. This provides a
        // solution without adding margins to the top/bottom of the view.
        setClipToPadding(false);
        setClipChildren(false);
    }

    void setDseIconTint(@Nullable ColorStateList tint) {
        ImageViewCompat.setImageTintList(mDseIconView, tint);
    }

    private void updateTouchDelegate() {
        if (mPlusButton.getVisibility() != View.VISIBLE) {
            if (mTouchDelegate != null) {
                setTouchDelegate(null);
                mTouchDelegate = null;
                mLastTouchDelegateRect = null;
            }
            return;
        }

        Rect bounds = new Rect();
        mPlusButton.getHitRect(bounds);
        if (bounds.isEmpty()) return;

        int minTargetSize = getResources().getDimensionPixelSize(R.dimen.min_touch_target_size);
        int widthDelta = Math.max(0, minTargetSize - bounds.width()) / 2;
        int heightDelta = Math.max(0, minTargetSize - bounds.height()) / 2;
        bounds.inset(-widthDelta, -heightDelta);

        if (bounds.equals(mLastTouchDelegateRect)) return;
        mLastTouchDelegateRect = bounds;

        mLastTouchDelegateRect = bounds;
        mTouchDelegate = new TouchDelegate(bounds, mPlusButton);
        setTouchDelegate(mTouchDelegate);
    }
}
