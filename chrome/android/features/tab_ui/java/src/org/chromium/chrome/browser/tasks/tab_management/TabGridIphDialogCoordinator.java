// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;

import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Coordinator for the IPH dialog in grid tab switcher.
 */
class TabGridIphDialogCoordinator implements TabSwitcherCoordinator.IphController {
    private final View mParentView;
    private final TabGridIphDialogView mIphDialogView;
    private final PropertyModel mModel;
    private final ModalDialogManager mModalDialogManager;
    private final ViewTreeObserver.OnGlobalLayoutListener mRootViewLayoutListener;

    TabGridIphDialogCoordinator(
            Context context, ViewGroup parent, ModalDialogManager modalDialogManager) {
        mIphDialogView = (TabGridIphDialogView) LayoutInflater.from(context).inflate(
                R.layout.iph_drag_and_drop_dialog_layout, null, false);
        mModalDialogManager = modalDialogManager;
        mParentView = parent;

        ModalDialogProperties.Controller dialogController = new ModalDialogProperties.Controller() {
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
            }
        };
        mModel = new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                         .with(ModalDialogProperties.CONTROLLER, dialogController)
                         .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                         .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                 context.getResources().getString(R.string.ok))
                         .with(ModalDialogProperties.CUSTOM_VIEW, mIphDialogView)
                         .build();

        mIphDialogView.setRootView(mParentView);
        mRootViewLayoutListener = mIphDialogView::updateLayout;
        mParentView.getViewTreeObserver().addOnGlobalLayoutListener(mRootViewLayoutListener);
    }

    @Override
    public void showIph() {
        mModalDialogManager.showDialog(mModel, ModalDialogManager.ModalDialogType.APP);
        mIphDialogView.startIPHAnimation();
    }

    /** Destroy the IPH component. */
    public void destroy() {
        mParentView.getViewTreeObserver().removeOnGlobalLayoutListener(mRootViewLayoutListener);
    }
}
