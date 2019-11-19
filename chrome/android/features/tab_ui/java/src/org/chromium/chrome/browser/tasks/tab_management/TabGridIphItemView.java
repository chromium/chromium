// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.app.Activity;
import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.drawable.Animatable;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.os.Handler;
import android.support.graphics.drawable.Animatable2Compat;
import android.support.graphics.drawable.AnimatedVectorDrawableCompat;
import android.support.v4.view.ViewCompat;
import android.support.v7.content.res.AppCompatResources;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.PopupWindow;
import android.widget.TextView;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.widget.ChromeImageView;

/**
 * Represents a view that works as the entrance for IPH in GridTabSwitcher.
 */
public class TabGridIphItemView extends FrameLayout {
    private final int mDialogSideMargin;
    private final int mDialogTextSideMargin;
    private final int mDialogTextTopMarginPortrait;
    private final int mDialogTextTopMarginLandscape;
    private final Context mContext;
    private View mIphDialogView;
    private TextView mShowIPHDialogButton;
    private TextView mCloseIPHDialogButton;
    private TextView mIphIntroduction;
    private ChromeImageView mCloseIPHEntranceButton;
    private ScrimView mScrimView;
    private ScrimView.ScrimParams mScrimParams;
    private Drawable mIphDrawable;
    private PopupWindow mIphWindow;
    private Animatable mIphAnimation;
    private Animatable2Compat.AnimationCallback mAnimationCallback;
    private MarginLayoutParams mDialogMarginParams;
    private MarginLayoutParams mTitleTextMarginParams;
    private MarginLayoutParams mDescriptionTextMarginParams;
    private MarginLayoutParams mCloseButtonMarginParams;
    private ComponentCallbacks mComponentCallbacks;

    public TabGridIphItemView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
        mDialogSideMargin =
                (int) mContext.getResources().getDimension(R.dimen.tab_grid_iph_dialog_side_margin);
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

        FrameLayout backgroundView = new FrameLayout(mContext);
        mIphDialogView = LayoutInflater.from(mContext).inflate(
                R.layout.iph_drag_and_drop_dialog_layout, backgroundView, false);
        mShowIPHDialogButton = findViewById(R.id.show_me_button);
        mCloseIPHEntranceButton = findViewById(R.id.close_iph_button);
        mIphIntroduction = findViewById(R.id.iph_description);
        Drawable closeButtonDrawable = getScaledCloseImageDrawable();
        mCloseIPHEntranceButton.setImageDrawable(closeButtonDrawable);
        mCloseIPHDialogButton = mIphDialogView.findViewById(R.id.close_button);
        mIphDrawable =
                ((ImageView) mIphDialogView.findViewById(R.id.animation_drawable)).getDrawable();
        mIphAnimation = (Animatable) mIphDrawable;
        TextView iphDialogTitleText = mIphDialogView.findViewById(R.id.title);
        TextView iphDialogDescriptionText = mIphDialogView.findViewById(R.id.description);
        mAnimationCallback = new Animatable2Compat.AnimationCallback() {
            @Override
            public void onAnimationEnd(Drawable drawable) {
                Handler handler = new Handler();
                handler.postDelayed(mIphAnimation::start, 1500);
            }
        };
        backgroundView.addView(mIphDialogView);
        mIphWindow = new PopupWindow(backgroundView, ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT);
        mScrimView = new ScrimView(mContext, null, backgroundView);
        mDialogMarginParams = (MarginLayoutParams) mIphDialogView.getLayoutParams();
        mTitleTextMarginParams = (MarginLayoutParams) iphDialogTitleText.getLayoutParams();
        mDescriptionTextMarginParams =
                (MarginLayoutParams) iphDialogDescriptionText.getLayoutParams();
        mCloseButtonMarginParams = (MarginLayoutParams) mCloseIPHDialogButton.getLayoutParams();

        mComponentCallbacks = new ComponentCallbacks() {
            @Override
            public void onConfigurationChanged(Configuration newConfig) {
                updateMargins(newConfig.orientation);
            }

            @Override
            public void onLowMemory() {}
        };
        mContext.registerComponentCallbacks(mComponentCallbacks);
    }

    /**
     * Setup the {@link View.OnClickListener} for close button in the entrance of IPH.
     *
     * @param listener The {@link View.OnClickListener} used to setup close button in IPH entrance.
     */
    void setupCloseIphEntranceButtonOnclickListener(View.OnClickListener listener) {
        mCloseIPHEntranceButton.setOnClickListener(listener);
    }

    /**
     * Setup the {@link View.OnClickListener} for close button in the IPH dialog.
     *
     * @param listener The {@link View.OnClickListener} used to setup close button in IPH dialog.
     */
    void setupCloseIphDialogButtonOnclickListener(View.OnClickListener listener) {
        mCloseIPHDialogButton.setOnClickListener(listener);
    }

    /**
     * Setup the {@link View.OnClickListener} for the button that shows the IPH dialog.
     *
     * @param listener The {@link View.OnClickListener} used to setup show IPH button.
     */
    void setupShowIphButtonOnclickListener(View.OnClickListener listener) {
        mShowIPHDialogButton.setOnClickListener(listener);
    }

    /**
     * Setup the {@link ScrimView.ScrimParams} for the {@link ScrimView}.
     *
     * @param observer The {@link ScrimView.ScrimObserver} used to setup the scrim view params.
     */
    void setupIPHDialogScrimViewObserver(ScrimView.ScrimObserver observer) {
        mScrimParams = new ScrimView.ScrimParams(mIphDialogView, false, true, 0, observer);
    }

    /**
     * Close the IPH dialog and hide the scrim view.
     */
    void closeIphDialog() {
        mIphWindow.dismiss();
        mScrimView.hideScrim(true);
        AnimatedVectorDrawableCompat.unregisterAnimationCallback(mIphDrawable, mAnimationCallback);
        mIphAnimation.stop();
    }

    /**
     * Show the scrim view and show dialog above it.
     */
    void showIPHDialog() {
        if (mScrimParams != null) {
            mScrimView.showScrim(mScrimParams);
        }
        updateMargins(mContext.getResources().getConfiguration().orientation);
        mIphWindow.showAtLocation((View) getParent(), Gravity.CENTER, 0, 0);
        AnimatedVectorDrawableCompat.registerAnimationCallback(mIphDrawable, mAnimationCallback);
        mIphAnimation.start();
    }

    private Drawable getScaledCloseImageDrawable() {
        // Scale down the close button image to 18dp.
        Context context = getContext();
        int size = (int) context.getResources().getDimension(
                R.dimen.tab_grid_iph_card_close_button_size);
        Drawable closeDrawable = AppCompatResources.getDrawable(getContext(), R.drawable.btn_close);
        Bitmap closeBitmap = ((BitmapDrawable) closeDrawable).getBitmap();
        return new BitmapDrawable(
                getResources(), Bitmap.createScaledBitmap(closeBitmap, size, size, true));
    }

    private void updateMargins(int orientation) {
        DisplayMetrics displayMetrics = new DisplayMetrics();
        ((Activity) getContext()).getWindowManager().getDefaultDisplay().getMetrics(displayMetrics);
        int screenHeight = displayMetrics.heightPixels;

        int dialogHeight =
                (int) mContext.getResources().getDimension(R.dimen.tab_grid_iph_dialog_height);
        // Dynamically setup the top margin base on screen height, the minimum top margin is
        // specified in case the screen height is smaller than or too close to dialog height.
        int updatedDialogTopMargin = Math.max((screenHeight - dialogHeight) / 2,
                (int) mContext.getResources().getDimension(R.dimen.tab_grid_iph_dialog_top_margin));

        int dialogTopMargin;
        int dialogSideMargin;
        int textTopMargin;
        if (orientation == Configuration.ORIENTATION_PORTRAIT) {
            dialogTopMargin = updatedDialogTopMargin;
            dialogSideMargin = mDialogSideMargin;
            textTopMargin = mDialogTextTopMarginPortrait;
        } else {
            dialogTopMargin = mDialogSideMargin;
            dialogSideMargin = updatedDialogTopMargin;
            textTopMargin = mDialogTextTopMarginLandscape;
        }
        mDialogMarginParams.setMargins(
                dialogSideMargin, dialogTopMargin, dialogSideMargin, dialogTopMargin);
        mTitleTextMarginParams.setMargins(
                mDialogTextSideMargin, textTopMargin, mDialogTextSideMargin, textTopMargin);
        mDescriptionTextMarginParams.setMargins(
                mDialogTextSideMargin, 0, mDialogTextSideMargin, textTopMargin);
        mCloseButtonMarginParams.setMargins(
                mDialogTextSideMargin, textTopMargin, mDialogTextSideMargin, textTopMargin);
    }

    /**
     * Updates color for inner views based on incognito mode.
     * @param isIncognito Whether the color is updated for incognito mode.
     */
    void updateColor(boolean isIncognito) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
            setBackground(
                    TabUiColorProvider.getCardViewBackgroundDrawable(getContext(), isIncognito));
        } else {
            ViewCompat.setBackgroundTintList(
                    this, TabUiColorProvider.getCardViewTintList(getContext(), isIncognito));
        }

        mShowIPHDialogButton.setTextAppearance(mShowIPHDialogButton.getContext(),
                isIncognito ? R.style.TextAppearance_BlueTitle2Incognito
                            : R.style.TextAppearance_BlueTitle2);
        mIphIntroduction.setTextAppearance(mIphIntroduction.getContext(),
                isIncognito ? R.style.TextAppearance_WhiteBody
                            : R.style.TextAppearance_BlackBodyDefault);

        ApiCompatibilityUtils.setImageTintList(mCloseIPHEntranceButton,
                TabUiColorProvider.getActionButtonTintList(
                        mCloseIPHEntranceButton.getContext(), isIncognito));
    }

    /**
     * Destroy any members that needs clean up.
     */
    public void destroy() {
        mContext.unregisterComponentCallbacks(mComponentCallbacks);
    }

    @VisibleForTesting
    PopupWindow getIphWindowForTesting() {
        return mIphWindow;
    }
}
