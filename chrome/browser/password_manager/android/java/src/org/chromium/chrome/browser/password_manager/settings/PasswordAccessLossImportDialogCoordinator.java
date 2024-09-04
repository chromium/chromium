// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import android.content.Context;
import android.content.res.Resources;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.loading_modal.LoadingModalDialogCoordinator;
import org.chromium.chrome.browser.password_manager.ManagePasswordsReferrer;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelper;
import org.chromium.components.sync.SyncService;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Shows the dialog instructing the user to import their passwords into the GMS Core after they've
 * been saved to the file on disk. It is displayed only if the user has up to date GMS Core
 * installed. If user accepts, it redirects the user to Google Password Manager (GMS Core).
 */
public class PasswordAccessLossImportDialogCoordinator {
    private class ModalDialogController implements ModalDialogProperties.Controller {

        @Override
        public void onClick(PropertyModel model, int buttonType) {
            if (buttonType == ButtonType.POSITIVE) {
                launchCredentialManager();
                mChromeShutDownRunnable.run();
            }
            mModalDialogManagerSupplier
                    .get()
                    .dismissDialog(
                            model,
                            buttonType == ButtonType.POSITIVE
                                    ? DialogDismissalCause.POSITIVE_BUTTON_CLICKED
                                    : DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
        }

        @Override
        public void onDismiss(PropertyModel model, int dismissalCause) {
            // This is called when the dialog is being dismissed and meant for cleanup. Nothing to
            // clean up here.
        }
    }

    private final Context mContext;
    private final SyncService mSyncService;
    private final Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    private final PasswordManagerHelper mPasswordManagerHelper;
    private final Runnable mChromeShutDownRunnable;

    public PasswordAccessLossImportDialogCoordinator(
            Context context,
            SyncService syncService,
            Supplier<ModalDialogManager> modalDialogManagerSupplier,
            PasswordManagerHelper passwordManagerHelper,
            Runnable chromeShutDownRunnable) {
        mContext = context;
        mSyncService = syncService;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mPasswordManagerHelper = passwordManagerHelper;
        mChromeShutDownRunnable = chromeShutDownRunnable;
    }

    public void showImportInstructionDialog() {
        Resources resources = mContext.getResources();
        PropertyModel model =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, new ModalDialogController())
                        .with(
                                ModalDialogProperties.TITLE,
                                resources,
                                org.chromium.chrome.browser.password_manager.R.string
                                        .access_loss_import_dialog_title)
                        .with(
                                ModalDialogProperties.MESSAGE_PARAGRAPH_1,
                                resources.getString(
                                        org.chromium.chrome.browser.password_manager.R.string
                                                .access_loss_import_dialog_desc))
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                resources,
                                org.chromium.chrome.browser.password_manager.R.string
                                        .access_loss_import_dialog_positive_button_text)
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                resources,
                                org.chromium.chrome.browser.password_manager.R.string.cancel)
                        .build();
        mModalDialogManagerSupplier.get().showDialog(model, ModalDialogType.APP);
    }

    private void launchCredentialManager() {
        // TODO (crbug.com/356851810): Add new referrer and replace CHROME_SETTINGS referrer with
        // the new one.
        LoadingModalDialogCoordinator loadingDialogCoordinator =
                LoadingModalDialogCoordinator.create(mModalDialogManagerSupplier, mContext);
        mPasswordManagerHelper.launchTheCredentialManager(
                ManagePasswordsReferrer.ACCESS_LOSS_WARNING,
                mSyncService,
                loadingDialogCoordinator,
                mModalDialogManagerSupplier,
                mContext,
                null);
    }
}
