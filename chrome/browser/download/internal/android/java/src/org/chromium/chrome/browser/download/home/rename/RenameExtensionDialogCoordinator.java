// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.download.home.rename;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.download.internal.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * The Coordinator for the Rename Extension Dialog. Manages UI objects like views and model, and
 * handles communication with the {@link ModalDialogManager}.
 */
@NullMarked
final class RenameExtensionDialogCoordinator {
    private final Context mContext;
    private final @Nullable ModalDialogManager mModalDialogManager;
    private final Callback<Boolean> mOnClickEventCallback;
    private final Callback<Integer> mOnDismissEventCallback;

    private @Nullable PropertyModel mRenameExtensionDialogModel;

    public RenameExtensionDialogCoordinator(
            Context context,
            @Nullable ModalDialogManager modalDialogManager,
            Callback<Boolean> onClickCallback,
            Callback</*DialogDismissalCause*/ Integer> dismissCallback) {
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mOnClickEventCallback = onClickCallback;
        mOnDismissEventCallback = dismissCallback;
        if (!ChromeFeatureList.sClankStartupTabOptimizations.isEnabled()) {
            ensureDialogInitialized();
        }
    }

    public void destroy() {
        dismissDialog(DialogDismissalCause.ACTIVITY_DESTROYED);
    }

    public void showDialog() {
        if (mModalDialogManager == null) return;
        ensureDialogInitialized();
        mModalDialogManager.showDialog(
                mRenameExtensionDialogModel, ModalDialogManager.ModalDialogType.APP, true);
    }

    public void dismissDialog(int dismissalCause) {
        if (mModalDialogManager != null && mRenameExtensionDialogModel != null) {
            mModalDialogManager.dismissDialog(mRenameExtensionDialogModel, dismissalCause);
        }
    }

    @EnsuresNonNull("mRenameExtensionDialogModel")
    private void ensureDialogInitialized() {
        if (mRenameExtensionDialogModel != null) {
            return;
        }
        View customView =
                LayoutInflater.from(mContext)
                        .inflate(R.layout.download_rename_extension_custom_dialog, null);
        mRenameExtensionDialogModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(
                                ModalDialogProperties.CONTROLLER,
                                new RenameExtensionDialogController())
                        .with(
                                ModalDialogProperties.TITLE,
                                mContext.getString(R.string.rename_extension_confirmation))
                        .with(ModalDialogProperties.CUSTOM_VIEW, customView)
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                mContext.getResources(),
                                R.string.confirm)
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                mContext.getResources(),
                                R.string.cancel)
                        .build();
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
