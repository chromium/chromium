// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.view.LayoutInflater;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * The coordinator for the password generation modal dialog. Manages the sub-component objects
 * and handles communication with the {@link ModalDialogManager}.
 */
public class PasswordGenerationDialogCoordinator {
    private final ModalDialogManager mModalDialogManager;
    private final PasswordGenerationDialogModel mModel;
    private final PasswordGenerationDialogCustomView mCustomView;

    private PropertyModel mDialogModel;

    public PasswordGenerationDialogCoordinator(@NonNull ChromeActivity activity) {
        mModel = new PasswordGenerationDialogModel();
        mCustomView = (PasswordGenerationDialogCustomView) LayoutInflater.from(activity).inflate(
                R.layout.password_generation_dialog, null);
        mModalDialogManager = activity.getModalDialogManager();
    }

    public void showDialog(String generatedPassword, String saveExplanationText,
            Callback<Boolean> onPasswordAcceptedOrRejected) {
        PasswordGenerationDialogMediator.initializeState(
                mModel, generatedPassword, saveExplanationText);
        PasswordGenerationDialogViewBinder.bind(mModel, mCustomView);

        mDialogModel = PasswordGenerationDialogMediator
                               .createDialogModelBuilder(onPasswordAcceptedOrRejected, mCustomView)
                               .build();
        mModalDialogManager.showDialog(mDialogModel, ModalDialogManager.ModalDialogType.APP);
    }

    public void dismissDialog(@DialogDismissalCause int dismissalCause) {
        mModalDialogManager.dismissDialog(mDialogModel, dismissalCause);
    }
}
