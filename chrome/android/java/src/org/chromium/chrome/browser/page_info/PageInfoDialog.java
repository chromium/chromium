// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.LinearLayout;
import android.widget.ScrollView;

import androidx.annotation.NonNull;

import org.chromium.ui.interpolators.BakedBezierInterpolator;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Represents the dialog containing the page info view.
 */
class PageInfoDialog {
    private static final int ENTER_START_DELAY_MS = 100;
    private static final int ENTER_EXIT_DURATION_MS = 200;
    private static final int CLOSE_CLEANUP_DELAY_MS = 10;

    @NonNull
    private final PageInfoView mView;
    private final boolean mIsSheet;
    // The dialog implementation.
    // mSheetDialog is set if the dialog appears as a sheet. Otherwise, mModalDialog is set.
    private final Dialog mSheetDialog;
    private final PropertyModel mModalDialogModel;
    @NonNull
    private final ModalDialogManager mManager;
    @NonNull
    private final ModalDialogProperties.Controller mController;

    // Animation which is currently running, if there is one.
    private Animator mCurrentAnimation;

    private boolean mDismissWithoutAnimation;

    /**
     * Creates a new page info dialog. The dialog can appear as a sheet (using Android dialogs) or a
     * standard dialog (using modal dialogs).
     *
     * @param context The context used for creating the dialog.
     * @param view The view shown inside the dialog.
     * @param tabView The view if the tab the dialog is shown in.
     * @param isSheet Whether the dialog should appear as a sheet.
     * @param manager The dialog's manager used for modal dialogs.
     * @param controller The dialog's controller.
     *
     */
    public PageInfoDialog(Context context, @NonNull PageInfoView view, View tabView,
            boolean isSheet, @NonNull ModalDialogManager manager,
            @NonNull ModalDialogProperties.Controller controller) {
        mView = view;
        mIsSheet = isSheet;
        mManager = manager;
        mController = controller;

        mView.setVisibility(View.INVISIBLE);
        mView.addOnLayoutChangeListener(new View.OnLayoutChangeListener() {
            @Override
            public void onLayoutChange(
                    View v, int l, int t, int r, int b, int ol, int ot, int or, int ob) {
                // Trigger the entrance animations once the main container has been laid out and has
                // a height.
                mView.removeOnLayoutChangeListener(this);
                mView.setVisibility(View.VISIBLE);
                createAllAnimations(true, null).start();
            }
        });

        ViewGroup container;
        if (isSheet) {
            // On smaller screens, make the dialog fill the width of the screen.
            container = createSheetContainer(context, tabView);
        } else {
            // On larger screens, modal dialog already has an maximum width set.
            container = new ScrollView(context);
        }

        container.addView(mView);

        if (isSheet) {
            mSheetDialog = createSheetDialog(context, container);
            mModalDialogModel = null;
        } else {
            mModalDialogModel = createModalDialog(container);
            mSheetDialog = null;
        }
    }

    /** Shows the dialogs. */
    public void show() {
        if (mIsSheet) {
            mSheetDialog.show();
        } else {
            mManager.showDialog(mModalDialogModel, ModalDialogManager.ModalDialogType.APP);
        }
    }

    /**
     * Hides the dialog.
     *
     * @param animated Whether to animate the transition to hidden.
     */
    public void dismiss(boolean animated) {
        mDismissWithoutAnimation = !animated;
        if (mIsSheet) {
            mSheetDialog.dismiss();
        } else {
            mManager.dismissDialog(mModalDialogModel, DialogDismissalCause.UNKNOWN);
        }
    }

    private Dialog createSheetDialog(Context context, View container) {
        Dialog sheetDialog = new Dialog(context) {
            private void superDismiss() {
                super.dismiss();
            }

            @Override
            public void dismiss() {
                if (mDismissWithoutAnimation) {
                    // Dismiss the modal dialogs without any custom animations.
                    super.dismiss();
                } else {
                    createAllAnimations(false, () -> {
                        // onAnimationEnd is called during the final frame of the animation.
                        // Delay the cleanup by a tiny amount to give this frame a chance to
                        // be displayed before we destroy the dialog.
                        mView.postDelayed(this ::superDismiss, CLOSE_CLEANUP_DELAY_MS);
                    }).start();
                }
            }
        };
        sheetDialog.requestWindowFeature(Window.FEATURE_NO_TITLE);
        sheetDialog.setCanceledOnTouchOutside(true);

        Window window = sheetDialog.getWindow();
        window.setGravity(Gravity.TOP);
        window.setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));

        sheetDialog.setOnDismissListener(new DialogInterface.OnDismissListener() {
            @Override
            public void onDismiss(DialogInterface dialog) {
                mController.onDismiss(null, DialogDismissalCause.UNKNOWN);
            }
        });

        sheetDialog.addContentView(container,
                new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT,
                        LinearLayout.LayoutParams.MATCH_PARENT));

        // This must be called after addContentView, or it won't fully fill to the edge.
        window.setLayout(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);

        return sheetDialog;
    }

    private PropertyModel createModalDialog(View container) {
        return new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                .with(ModalDialogProperties.CONTROLLER, mController)
                .with(ModalDialogProperties.CUSTOM_VIEW, container)
                .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                .build();
    }

    private ViewGroup createSheetContainer(Context context, View tabView) {
        return new ScrollView(context) {
            @Override
            protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
                heightMeasureSpec = MeasureSpec.makeMeasureSpec(
                        tabView != null ? tabView.getHeight() : 0, MeasureSpec.AT_MOST);
                super.onMeasure(widthMeasureSpec, heightMeasureSpec);
            }
        };
    }

    /**
     * Create an animator to slide in the entire dialog from the top of the screen.
     */
    private Animator createDialogSlideAnimaton(boolean isEnter) {
        final float animHeight = -mView.getHeight();
        ObjectAnimator translateAnim;
        if (isEnter) {
            mView.setTranslationY(animHeight);
            translateAnim = ObjectAnimator.ofFloat(mView, View.TRANSLATION_Y, 0f);
            translateAnim.setInterpolator(BakedBezierInterpolator.FADE_IN_CURVE);
        } else {
            translateAnim = ObjectAnimator.ofFloat(mView, View.TRANSLATION_Y, animHeight);
            translateAnim.setInterpolator(BakedBezierInterpolator.FADE_OUT_CURVE);
        }
        translateAnim.setDuration(ENTER_EXIT_DURATION_MS);
        return translateAnim;
    }

    /**
     * Create an animator to show/hide the entire dialog. On phones the dialog is slid in as a
     * sheet. Otherwise, the default fade-in is used.
     */
    private Animator createAllAnimations(boolean isEnter, Runnable onAnimationEnd) {
        Animator dialogAnimation =
                mIsSheet ? createDialogSlideAnimaton(isEnter) : new AnimatorSet();
        Animator viewAnimation = mView.createEnterExitAnimation(isEnter);
        AnimatorSet allAnimations = new AnimatorSet();
        if (isEnter) allAnimations.setStartDelay(ENTER_START_DELAY_MS);
        allAnimations.playTogether(dialogAnimation, viewAnimation);
        allAnimations.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                mCurrentAnimation = null;
                if (onAnimationEnd == null) return;
                onAnimationEnd.run();
            }
        });
        if (mCurrentAnimation != null) mCurrentAnimation.cancel();
        mCurrentAnimation = allAnimations;
        return allAnimations;
    }
}