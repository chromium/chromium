// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.content.res.Configuration;
import android.graphics.drawable.Animatable;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.vectordrawable.graphics.drawable.Animatable2Compat;
import androidx.vectordrawable.graphics.drawable.AnimatedVectorDrawableCompat;

import org.chromium.chrome.tab_ui.R;

/**
 * The view for TabGridIph related UIs.
 */
public class TabGridIphDialogView extends LinearLayout {
    private final int mDialogHeight;
    private final int mDialogTopMargin;
    private final int mDialogTextSideMargin;
    private final int mDialogTextTopMarginPortrait;
    private final int mDialogTextTopMarginLandscape;
    private final Context mContext;
    private View mRootView;
    private Drawable mIphDrawable;
    private Animatable mIphAnimation;
    private Animatable2Compat.AnimationCallback mAnimationCallback;
    private ViewGroup.MarginLayoutParams mTitleTextMarginParams;
    private ViewGroup.MarginLayoutParams mDescriptionTextMarginParams;
    private int mParentViewHeight;

    public TabGridIphDialogView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
        mDialogHeight =
                (int) mContext.getResources().getDimension(R.dimen.tab_grid_iph_dialog_height);
        mDialogTopMargin =
                (int) mContext.getResources().getDimension(R.dimen.tab_grid_iph_dialog_top_margin);
        mDialogTextSideMargin = (int) mContext.getResources().getDimension(
                R.dimen.tab_grid_iph_dialog_text_side_margin);
        mDialogTextTopMarginPortrait = (int) mContext.getResources().getDimension(
                R.dimen.tab_grid_iph_dialog_text_top_margin_portrait);
        mDialogTextTopMarginLandscape = (int) mContext.getResources().getDimension(
                R.dimen.tab_grid_iph_dialog_text_top_margin_landscape);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mIphDrawable = ((ImageView) findViewById(R.id.animation_drawable)).getDrawable();
        mIphAnimation = (Animatable) mIphDrawable;
        TextView iphDialogTitleText = findViewById(R.id.title);
        TextView iphDialogDescriptionText = findViewById(R.id.description);
        mAnimationCallback = new Animatable2Compat.AnimationCallback() {
            @Override
            public void onAnimationEnd(Drawable drawable) {
                Handler handler = new Handler();
                handler.postDelayed(mIphAnimation::start, 1500);
            }
        };
        mTitleTextMarginParams =
                (ViewGroup.MarginLayoutParams) iphDialogTitleText.getLayoutParams();
        mDescriptionTextMarginParams =
                (ViewGroup.MarginLayoutParams) iphDialogDescriptionText.getLayoutParams();
    }

    /**
     * Setup the root view of the dialog.
     * @param rootView  The root view of the IPH dialog. Will be used to update the IPH view layout.
     */
    void setRootView(View rootView) {
        mRootView = rootView;
    }

    /**
     * Stops the IPH animation. This is called when the IPH dialog hides.
     */
    void stopIPHAnimation() {
        AnimatedVectorDrawableCompat.unregisterAnimationCallback(mIphDrawable, mAnimationCallback);
        mIphAnimation.stop();
    }

    /**
     * Update the IPH view layout and start playing IPH animation. This is called when the IPH
     * dialog shows.
     */
    void startIPHAnimation() {
        updateLayout();
        AnimatedVectorDrawableCompat.registerAnimationCallback(mIphDrawable, mAnimationCallback);
        mIphAnimation.start();
    }

    /**
     * Update the IPH view layout based on the current size of the root view.
     */
    void updateLayout() {
        if (mParentViewHeight == mRootView.getHeight()) return;
        mParentViewHeight = mRootView.getHeight();
        int orientation = mContext.getResources().getConfiguration().orientation;
        int textTopMargin;
        if (orientation == Configuration.ORIENTATION_PORTRAIT) {
            textTopMargin = mDialogTextTopMarginPortrait;
        } else {
            textTopMargin = mDialogTextTopMarginLandscape;
        }
        mTitleTextMarginParams.setMargins(
                mDialogTextSideMargin, textTopMargin, mDialogTextSideMargin, textTopMargin);
        mDescriptionTextMarginParams.setMargins(
                mDialogTextSideMargin, 0, mDialogTextSideMargin, textTopMargin);

        // The IPH view height should be at most (root view height - 2 * top margin).
        int dialogHeight = Math.min(mDialogHeight, mParentViewHeight - 2 * mDialogTopMargin);
        setMinimumHeight(dialogHeight);
    }
}
