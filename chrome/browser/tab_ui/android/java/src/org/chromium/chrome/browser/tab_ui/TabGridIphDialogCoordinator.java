// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;

import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.TraceEvent;
import org.chromium.components.browser_ui.widget.IphDialogView;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Coordinator for the IPH dialog in grid tab switcher. */
public class TabGridIphDialogCoordinator implements TabSwitcherIphController {
    private final IphDialogView mIphDialogView;
    private final PropertyModel mModel;
    private final ModalDialogManager mModalDialogManager;
    private final ViewTreeObserver.OnGlobalLayoutListener mRootViewLayoutListener;

    private @Nullable ViewGroup mParentView;
    private boolean mGlobalLayoutListenerAttached;

    public TabGridIphDialogCoordinator(Context context, ModalDialogManager modalDialogManager) {
        try (TraceEvent e = TraceEvent.scoped("TabGridIphDialogCoordinator.constructor")) {
            mIphDialogView =
                    (IphDialogView)
                            LayoutInflater.from(context)
                                    .inflate(R.layout.iph_dialog_layout, null, false);
            mIphDialogView.setDrawable(
                    AppCompatResources.getDrawable(
                            context, R.drawable.iph_drag_and_drop_animated_drawable),
                    context.getResources().getString(R.string.iph_drag_and_drop_content));
            mIphDialogView.setTitle(
                    context.getResources().getString(R.string.iph_drag_and_drop_title));
            mIphDialogView.setDescription(
                    context.getResources().getString(R.string.iph_drag_and_drop_content));
            mModalDialogManager = modalDialogManager;

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
                                    context.getResources().getString(R.string.ok))
                            .with(ModalDialogProperties.CUSTOM_VIEW, mIphDialogView)
                            .build();

            mRootViewLayoutListener = mIphDialogView::updateLayout;
        }
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

    @Override
    public void showIph() {
        if (mParentView == null) return;

        attachParentGlobalLayoutListener();
        mModalDialogManager.showDialog(mModel, ModalDialogManager.ModalDialogType.APP);
        mIphDialogView.startIPHAnimation();
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
