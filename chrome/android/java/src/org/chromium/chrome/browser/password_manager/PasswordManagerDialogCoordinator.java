// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.DrawableRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * The coordinator for the password manager illustration modal dialog. Manages the sub-component
 * objects.
 */
public class PasswordManagerDialogCoordinator {
    private final PasswordManagerDialogMediator mMediator;

    PasswordManagerDialogCoordinator(Context context, ModalDialogManager modalDialogManager,
            View androidContentView, ChromeFullscreenManager fullscreenManager,
            int containerHeightResource) {
        PropertyModel mModel = PasswordManagerDialogProperties.defaultModelBuilder().build();
        View customView =
                LayoutInflater.from(context).inflate(R.layout.password_manager_dialog, null);
        mMediator = new PasswordManagerDialogMediator(mModel, createDialogModelBuilder(customView),
                modalDialogManager, androidContentView, customView.getResources(),
                fullscreenManager, containerHeightResource);
        PropertyModelChangeProcessor.create(
                mModel, customView, PasswordManagerDialogViewBinder::bind);
    }

    public void showDialog(String title, String details, int boldRangeStart, int boldRangeEnd,
            @DrawableRes int drawableId, String positiveButtonText, String negativeButtonText,
            Callback<Integer> onClick, boolean primaryButtonFilled,
            @ModalDialogManager.ModalDialogType int type) {
        mMediator.setContents(title, details, boldRangeStart, boldRangeEnd, drawableId);
        mMediator.setButtons(positiveButtonText, negativeButtonText, onClick, primaryButtonFilled);
        mMediator.showDialog(type);
    }

    public void showDialog(String title, String details, @DrawableRes int drawableId,
            String positiveButtonText, String negativeButtonText, Callback<Integer> onClick,
            boolean primaryButtonFilled, @ModalDialogManager.ModalDialogType int type) {
        showDialog(title, details, 0, 0, drawableId, positiveButtonText, negativeButtonText,
                onClick, primaryButtonFilled, type);
    }

    public void dismissDialog(@DialogDismissalCause int dismissalCause) {
        mMediator.dismissDialog(dismissalCause);
    }

    private static PropertyModel.Builder createDialogModelBuilder(View customView) {
        return new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                .with(ModalDialogProperties.CUSTOM_VIEW, customView);
    }

    @VisibleForTesting
    public PasswordManagerDialogMediator getMediatorForTesting() {
        return mMediator;
    }
}
