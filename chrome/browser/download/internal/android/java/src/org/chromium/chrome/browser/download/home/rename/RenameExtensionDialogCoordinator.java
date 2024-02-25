// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.download.home.rename;

import android.content.Context;
import android.view.LayoutInflater;
import android.widget.ScrollView;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.download.internal.R;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * The Coordinator for the Rename Extension Dialog. Manages UI objects like views and model, and
 * handles communication with the {@link ModalDialogManager}.
 */
final class RenameExtensionDialogCoordinator {
    private final ModalDialogManager mModalDialogManager;
    private final ScrollView mRenameExtensionDialogCustomView;
    private final PropertyModel mRenameExtensionDialogModel;
    private final Callback<Boolean> mOnClickEventCallback;
    private final Callback<Integer> mOnDismissEventCallback;

    public RenameExtensionDialogCoordinator(
            Context context,
            ModalDialogManager modalDialogManager,
            Callback<Boolean> onClickCallback,
            Callback</*DialogDismissalCause*/ Integer> dismissCallback) {
        mModalDialogManager = modalDialogManager;
        mRenameExtensionDialogCustomView =
                (ScrollView)
                        LayoutInflater.from(context)
                                .inflate(R.layout.download_rename_extension_custom_dialog, null);
        mRenameExtensionDialogModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(
                                ModalDialogProperties.CONTROLLER,
                                new RenameExtensionDialogController())
                        .with(
                                ModalDialogProperties.TITLE,
                                context.getString(R.string.rename_extension_confirmation))
                        .with(ModalDialogProperties.CUSTOM_VIEW, mRenameExtensionDialogCustomView)
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                context.getResources(),
                                R.string.confirm)
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                context.getResources(),
                                R.string.cancel)
                        .build();

        mOnClickEventCallback = onClickCallback;
        mOnDismissEventCallback = dismissCallback;
    }

    public void destroy() {
        dismissDialog(DialogDismissalCause.ACTIVITY_DESTROYED);
    }

    public void showDialog() {
        mModalDialogManager.showDialog(
                mRenameExtensionDialogModel, ModalDialogManager.ModalDialogType.APP, true);
    }

    public void dismissDialog(int dismissalCause) {
        if (mModalDialogManager != null) {
            mModalDialogManager.dismissDialog(mRenameExtensionDialogModel, dismissalCause);
        }
    }

    private class RenameExtensionDialogController implements ModalDialogProperties.Controller {
        @Override
        public void onDismiss(PropertyModel model, int dismissalCause) {
            mOnDismissEventCallback.onResult(dismissalCause);
        }

        @Override
        public void onClick(PropertyModel model, int buttonType) {
            switch (buttonType) {
                case ModalDialogProperties.ButtonType.POSITIVE:
                    mOnClickEventCallback.onResult(true);
                    break;
                case ModalDialogProperties.ButtonType.NEGATIVE:
                    mOnClickEventCallback.onResult(false);
                    break;
                default:
            }
        }
    }
}
