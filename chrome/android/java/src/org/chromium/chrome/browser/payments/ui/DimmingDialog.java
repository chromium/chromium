// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.ui;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.app.Activity;
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.os.Build;
import android.view.Gravity;
import android.view.View;
import android.view.View.OnLayoutChangeListener;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.view.Window;
import android.widget.FrameLayout;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.widget.AlwaysDismissedDialog;
import org.chromium.chrome.browser.ui.widget.animation.AnimatorProperties;
import org.chromium.chrome.browser.ui.widget.animation.Interpolators;
import org.chromium.chrome.browser.util.ColorUtils;

import java.util.ArrayList;
import java.util.Collection;

/**
 * A fullscreen semitransparent dialog used for dimming Chrome when overlaying a bottom sheet
 * dialog/CCT or an alert dialog on top of it. FLAG_DIM_BEHIND is not being used because it causes
 * the web contents of a payment handler CCT to also dim on some versions of Android (e.g., Nougat).
 *
 * Note: Do not use this class outside of the payments.ui package!
 * TODO(crbug.com/806868): Revert the visibility to package default again when it is no longer used
 * by Autofill Assistant.
 */
/* package */ class DimmingDialog {
    /**
     * Length of the animation to either show the UI or expand it to full height. Note that click of
     * 'Pay' button in PaymentRequestUI is not accepted until the animation is done, so this
     * duration also serves the function of preventing the user from accidentally double-clicking on
     * the screen when triggering payment and thus authorizing unwanted transaction.
     */
    private static final int DIALOG_ENTER_ANIMATION_MS = 225;

    /** Length of the animation to hide the bottom sheet UI. */
    private static final int DIALOG_EXIT_ANIMATION_MS = 195;

    private final Dialog mDialog;
    private final ViewGroup mFullContainer;
    private final int mAnimatorTranslation;
    private boolean mIsAnimatingDisappearance;

    /**
     * Builds the dimming dialog.
     *
     * @param activity        The activity on top of which the dialog should be displayed.
     * @param dismissListener The listener for the dismissal of this dialog.
     */
    /* package */ DimmingDialog(
            Activity activity, DialogInterface.OnDismissListener dismissListener) {
        // To handle the specced animations, the dialog is entirely contained within a translucent
        // FrameLayout. This could eventually be converted to a real BottomSheetDialog, but that
        // requires exploration of how interactions would work when the dialog can be sent back and
        // forth between the peeking and expanded state.
        mFullContainer = new FrameLayout(activity);
        mFullContainer.setBackgroundColor(ApiCompatibilityUtils.getColor(
                activity.getResources(), R.color.modal_dialog_scrim_color));
        mDialog = new AlwaysDismissedDialog(activity, R.style.DimmingDialog);
        mDialog.setOnDismissListener(dismissListener);
        mDialog.addContentView(mFullContainer,
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        Window dialogWindow = mDialog.getWindow();
        dialogWindow.setGravity(Gravity.CENTER);
        dialogWindow.setLayout(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
        dialogWindow.setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
        setVisibleStatusBarIconColor(dialogWindow);

        mAnimatorTranslation =
                activity.getResources().getDimensionPixelSize(R.dimen.payments_ui_translation);
    }

    /**
     * Makes sure that the color of the icons in the status bar makes the icons visible.
     * @param window The window whose status bar icon color is being set.
     */
    /* package */ static void setVisibleStatusBarIconColor(Window window) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) return;
        ApiCompatibilityUtils.setStatusBarIconColor(window.getDecorView().getRootView(),
                !ColorUtils.shouldUseLightForegroundOnBackground(window.getStatusBarColor()));
    }

    /** @param bottomSheetView The view to show in the bottom sheet. */
    /* package */ void addBottomSheetView(View bottomSheetView) {
        FrameLayout.LayoutParams bottomSheetParams =
                new FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT);
        bottomSheetParams.gravity = Gravity.CENTER_HORIZONTAL | Gravity.BOTTOM;
        mFullContainer.addView(bottomSheetView, bottomSheetParams);
        bottomSheetView.addOnLayoutChangeListener(new FadeInAnimator());
    }

    /** Show the dialog. */
    /* package */ void show() {
        mDialog.show();
    }

    /** Hide the dialog without dismissing it. */
    /* package */ void hide() {
        mDialog.hide();
    }

    /**
     * Dismiss the dialog.
     *
     * @param isAnimated If true, the dialog dismissal is animated.
     */
    /* package */ void dismiss(boolean isAnimated) {
        if (!mDialog.isShowing()) return;
        if (isAnimated) {
            new DisappearingAnimator(true);
        } else {
            mDialog.dismiss();
        }
    }

    /** @param overlay The overlay to show. This can be an error dialog, for example. */
    /* package */ void showOverlay(View overlay) {
        // Animate the bottom sheet going away.
        new DisappearingAnimator(false);

        int floatingDialogWidth = DimmingDialog.computeMaxWidth(mFullContainer.getContext(),
                mFullContainer.getMeasuredWidth(), mFullContainer.getMeasuredHeight());
        FrameLayout.LayoutParams overlayParams =
                new FrameLayout.LayoutParams(floatingDialogWidth, LayoutParams.WRAP_CONTENT);
        overlayParams.gravity = Gravity.CENTER;
        mFullContainer.addView(overlay, overlayParams);
    }

    /** @return Whether the dialog is currently animating disappearance. */
    /* package */ boolean isAnimatingDisappearance() {
        return mIsAnimatingDisappearance;
    }

    /**
     * Computes the maximum possible width for a dialog box.
     *
     * Follows https://www.google.com/design/spec/components/dialogs.html#dialogs-simple-dialogs
     *
     * @param context         Context to pull resources from.
     * @param availableWidth  Available width for the dialog.
     * @param availableHeight Available height for the dialog.
     * @return Maximum possible width for the dialog box.
     *
     * TODO(dfalcantara): Revisit this function when the new assets come in.
     * TODO(dfalcantara): The dialog should listen for configuration changes and resize accordingly.
     */
    private static int computeMaxWidth(Context context, int availableWidth, int availableHeight) {
        int baseUnit = context.getResources().getDimensionPixelSize(R.dimen.dialog_width_unit);
        int maxSize = Math.min(availableWidth, availableHeight);
        int multiplier = maxSize / baseUnit;
        return multiplier * baseUnit;
    }

    /**
     * Animates the whole dialog fading in and darkening everything else on screen.
     * This particular animation is not tracked because it is not meant to be cancellable.
     */
    private class FadeInAnimator extends AnimatorListenerAdapter implements OnLayoutChangeListener {
        @Override
        public void onLayoutChange(View v, int left, int top, int right, int bottom, int oldLeft,
                int oldTop, int oldRight, int oldBottom) {
            mFullContainer.getChildAt(0).removeOnLayoutChangeListener(this);

            Animator scrimFader = ObjectAnimator.ofInt(mFullContainer.getBackground(),
                    AnimatorProperties.DRAWABLE_ALPHA_PROPERTY, 0, 255);
            Animator alphaAnimator = ObjectAnimator.ofFloat(mFullContainer, View.ALPHA, 0f, 1f);

            AnimatorSet alphaSet = new AnimatorSet();
            alphaSet.playTogether(scrimFader, alphaAnimator);
            alphaSet.setDuration(DIALOG_ENTER_ANIMATION_MS);
            alphaSet.setInterpolator(Interpolators.LINEAR_OUT_SLOW_IN_INTERPOLATOR);
            alphaSet.start();
        }
    }

    /** Animates the bottom sheet (and optionally, the scrim) disappearing off screen. */
    private class DisappearingAnimator extends AnimatorListenerAdapter {
        private final boolean mIsDialogClosing;

        public DisappearingAnimator(boolean removeDialog) {
            mIsDialogClosing = removeDialog;

            Collection<Animator> animators = new ArrayList<>();

            View child = mFullContainer.getChildAt(0);
            if (child != null) {
                // Sheet fader.
                animators.add(ObjectAnimator.ofFloat(child, View.ALPHA, child.getAlpha(), 0f));
                // Sheet translator.
                animators.add(ObjectAnimator.ofFloat(
                        child, View.TRANSLATION_Y, 0f, mAnimatorTranslation));
            }

            if (mIsDialogClosing) {
                // Scrim fader.
                animators.add(ObjectAnimator.ofInt(mFullContainer.getBackground(),
                        AnimatorProperties.DRAWABLE_ALPHA_PROPERTY, 127, 0));
            }

            if (animators.isEmpty()) return;

            mIsAnimatingDisappearance = true;

            AnimatorSet current = new AnimatorSet();
            current.setDuration(DIALOG_EXIT_ANIMATION_MS);
            current.setInterpolator(Interpolators.FAST_OUT_LINEAR_IN_INTERPOLATOR);
            current.playTogether(animators);
            current.addListener(this);
            current.start();
        }

        @Override
        public void onAnimationEnd(Animator animation) {
            mIsAnimatingDisappearance = false;
            mFullContainer.removeView(mFullContainer.getChildAt(0));
            if (mIsDialogClosing && mDialog.isShowing()) mDialog.dismiss();
        }
    }

    @VisibleForTesting
    public Dialog getDialogForTest() {
        return mDialog;
    }
}
