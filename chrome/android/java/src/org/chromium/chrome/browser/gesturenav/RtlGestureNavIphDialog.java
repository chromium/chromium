// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnAttachStateChangeListener;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.chrome.R;
import org.chromium.components.browser_ui.widget.IphDialogView;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonStyles;
import org.chromium.ui.modelutil.PropertyModel;

/** Dialog of educating users on how to navigate by gesture in RTL mode. */
public class RtlGestureNavIphDialog {

    private final IphDialogView mIphDialogView;
    private final PropertyModel mModel;
    private final ModalDialogManager mModalDialogManager;

    private final ViewTreeObserver.OnGlobalLayoutListener mRootViewLayoutListener;

    private @Nullable ViewGroup mParentView;
    private boolean mGlobalLayoutListenerAttached;

    public RtlGestureNavIphDialog(
            Context context, ModalDialogManager modalDialogManager, Runnable dismissed) {
        mIphDialogView =
                (IphDialogView)
                        LayoutInflater.from(context)
                                .inflate(R.layout.iph_dialog_layout, null, false);
        mModalDialogManager = modalDialogManager;
        mIphDialogView.setDrawable(
                AppCompatResources.getDrawable(
                        context, R.drawable.rtl_gesture_nav_iph_dialog_drawable),
                context.getResources().getString(R.string.rtl_gesture_nav_iph_dialog_content));
        mIphDialogView.setTitle(
                context.getResources().getString(R.string.rtl_gesture_nav_iph_dialog_title));
        mIphDialogView.setDescription(
                context.getResources().getString(R.string.rtl_gesture_nav_iph_dialog_content));
        mIphDialogView.setIntervalMs(1200);

        ModalDialogProperties.Controller dialogController =
                new ModalDialogProperties.Controller() {
                    @Override
                    public void onClick(PropertyModel model, int buttonType) {
                        if (buttonType == ModalDialogProperties.ButtonType.POSITIVE) {
                            modalDialogManager.dismissDialog(
                                    model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                        }
                    }

                    @Override
                    public void onDismiss(PropertyModel model, int dismissalCause) {
                        dismissed.run();
                        mIphDialogView.stopIPHAnimation();
                        detachParentGlobalLayoutListener();
                    }
                };
        mModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, dialogController)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                context.getResources().getString(R.string.got_it))
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                ButtonStyles.PRIMARY_FILLED_NO_NEGATIVE)
                        .with(ModalDialogProperties.CUSTOM_VIEW, mIphDialogView)
                        .build();

        mRootViewLayoutListener = mIphDialogView::updateLayout;
    }

    /** Sets the parent view of the model dialog. */
    public void setParentView(@Nullable ViewGroup parentView) {
        boolean wasGlobalLayoutListenerAttached = mGlobalLayoutListenerAttached;
        detachParentGlobalLayoutListener();
        mParentView = parentView;
        mIphDialogView.setRootView(parentView);
        if (wasGlobalLayoutListenerAttached && parentView != null) {
            attachParentGlobalLayoutListener();
        }
    }

    public void show() {
        attachParentGlobalLayoutListener();
        mModalDialogManager.showDialog(mModel, ModalDialogManager.ModalDialogType.APP);
        mIphDialogView.addOnAttachStateChangeListener(
                new OnAttachStateChangeListener() {
                    @Override
                    public void onViewAttachedToWindow(@NonNull View v) {
                        mIphDialogView.startIPHAnimation();
                        mIphDialogView.removeOnAttachStateChangeListener(this);
                    }

                    @Override
                    public void onViewDetachedFromWindow(@NonNull View v) {}
                });
    }

    /** Destroy the IPH component. */
    public void destroy() {
        detachParentGlobalLayoutListener();
    }

    private void attachParentGlobalLayoutListener() {
        assert mParentView != null;
        mGlobalLayoutListenerAttached = true;
        mParentView.getViewTreeObserver().addOnGlobalLayoutListener(mRootViewLayoutListener);
    }

    private void detachParentGlobalLayoutListener() {
        if (mParentView == null) return;

        mGlobalLayoutListenerAttached = false;
        mParentView.getViewTreeObserver().removeOnGlobalLayoutListener(mRootViewLayoutListener);
    }
}
