// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.ui;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.ui.UiUtils;
import org.chromium.ui.drawable.AnimationLooper;
import org.chromium.ui.widget.ButtonCompat;

/** View that wraps signin screen and caches references to UI elements. */
public class SigninView extends LinearLayout {
    private SigninScrollView mScrollView;
    private ImageView mHeaderImage;
    private TextView mTitle;
    private View mAccountPicker;
    private ImageView mAccountImage;
    private TextView mAccountTextPrimary;
    private TextView mAccountTextSecondary;
    private ImageView mAccountPickerEndImage;
    private TextView mSyncTitle;
    private TextView mSyncDescription;
    private TextView mTapToSearchTitle;
    private TextView mTapToSearchDescription;
    private TextView mSafeBrowsingTitle;
    private TextView mSafeBrowsingDescription;
    private TextView mDetailsDescription;
    private ButtonCompat mAcceptButton;
    private Button mRefuseButton;
    private Button mMoreButton;
    private View mAcceptButtonEndPadding;
    private AnimationLooper mAnimationLooper;

    public SigninView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mScrollView = findViewById(R.id.signin_scroll_view);
        mHeaderImage = findViewById(R.id.signin_header_image);
        mTitle = findViewById(R.id.signin_title);
        mAccountPicker = findViewById(R.id.signin_account_picker);
        mAccountImage = findViewById(R.id.account_image);
        mAccountTextPrimary = findViewById(R.id.account_text_primary);
        mAccountTextSecondary = findViewById(R.id.account_text_secondary);
        mAccountPickerEndImage = findViewById(R.id.account_picker_end_image);
        mSyncTitle = findViewById(R.id.signin_sync_title);
        mSyncDescription = findViewById(R.id.signin_sync_description);
        mDetailsDescription = findViewById(R.id.signin_details_description);
        mAcceptButton = findViewById(R.id.positive_button);
        mRefuseButton = findViewById(R.id.negative_button);
        mMoreButton = findViewById(R.id.more_button);
        mAcceptButtonEndPadding = findViewById(R.id.positive_button_end_padding);

        mAnimationLooper = new AnimationLooper(mHeaderImage.getDrawable());
    }

    /**
     * TODO(crbug.com/1155123) Change the method to package private after modularization.
     */
    public SigninScrollView getScrollView() {
        return mScrollView;
    }

    /**
     * TODO(crbug.com/1155123) Change the method to package private after modularization.
     */
    public TextView getTitleView() {
        return mTitle;
    }

    /**
     * TODO(crbug.com/1155123) Change the method to package private after modularization.
     */
    public View getAccountPickerView() {
        return mAccountPicker;
    }

    /**
     * TODO(crbug.com/1155123) Change the method to package private after modularization.
     */
    public ImageView getAccountImageView() {
        return mAccountImage;
    }

    /**
     * TODO(crbug.com/1155123) Change the method to package private after modularization.
     */
    public TextView getAccountTextPrimary() {
        return mAccountTextPrimary;
    }

    /**
     * TODO(crbug.com/1155123) Change the method to package private after modularization.
     */
    public TextView getAccountTextSecondary() {
        return mAccountTextSecondary;
    }

    /**
     * TODO(crbug.com/1155123) Change the method to package private after modularization.
     */
    public ImageView getAccountPickerEndImageView() {
        return mAccountPickerEndImage;
    }

    /**
     * TODO(crbug.com/1155123) Change the method to package private after modularization.
     */
    public TextView getSyncTitleView() {
        return mSyncTitle;
    }

    /**
     * TODO(crbug.com/1155123) Change the method to package private after modularization.
     */
    public TextView getSyncDescriptionView() {
        return mSyncDescription;
    }

    /**
     * TODO(crbug.com/1155123) Change the method to package private after modularization.
     */
    public TextView getDetailsDescriptionView() {
        return mDetailsDescription;
    }

    /**
     * TODO(crbug.com/1155123) Change the method to package private after modularization.
     */
    public ButtonCompat getAcceptButton() {
        return mAcceptButton;
    }

    /**
     * TODO(crbug.com/1155123) Change the method to package private after modularization.
     */
    public Button getRefuseButton() {
        return mRefuseButton;
    }

    /**
     * TODO(crbug.com/1155123) Change the method to package private after modularization.
     */
    public Button getMoreButton() {
        return mMoreButton;
    }

    /**
     * TODO(crbug.com/1155123) Change the method to package private after modularization.
     */
    public View getAcceptButtonEndPadding() {
        return mAcceptButtonEndPadding;
    }

    /**
     * TODO(crbug.com/1155123) Change the method to package private after modularization.
     */
    public void startAnimations() {
        mAnimationLooper.start();
    }

    /**
     * TODO(crbug.com/1155123) Change the method to package private after modularization.
     */
    public void stopAnimations() {
        mAnimationLooper.stop();
    }

    /**
     * TODO(crbug.com/1155123) Change the method to package private after modularization.
     */
    public static Drawable getExpandArrowDrawable(Context context) {
        return UiUtils.getTintedDrawable(context, R.drawable.ic_expand_more_black_24dp,
                R.color.default_icon_color_tint_list);
    }

    /**
     * TODO(crbug.com/1155123) Change the method to package private after modularization.
     */
    public static Drawable getCheckmarkDrawable(Context context) {
        return AppCompatResources.getDrawable(context, R.drawable.ic_check_googblue_24dp);
    }
}
