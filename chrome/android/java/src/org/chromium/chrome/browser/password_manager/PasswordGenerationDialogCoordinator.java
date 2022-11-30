// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.app.Activity;
import android.view.LayoutInflater;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.ui.base.WindowAndroid;
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

    public PasswordGenerationDialogCoordinator(@NonNull WindowAndroid windowAndroid) {
        mModel = new PasswordGenerationDialogModel();
        mModalDialogManager = windowAndroid.getModalDialogManager();

        Activity activity = windowAndroid.getActivity().get();
        assert activity != null;
        mCustomView = (PasswordGenerationDialogCustomView) LayoutInflater.from(activity).inflate(
                R.layout.password_generation_dialog, null);
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
