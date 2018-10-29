// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.modaldialog.DialogDismissalCause;
import org.chromium.chrome.browser.modaldialog.ModalDialogView;

/** Class responsible for binding the model and the view. On bind, it lazily initializes the view
 * since all the needed data was made available at this point.
 */
public class PasswordGenerationDialogViewBinder {
    private static class PasswordGenerationDialogController implements ModalDialogView.Controller {
        private final Callback<Boolean> mPasswordActionCallback;

        public PasswordGenerationDialogController(Callback<Boolean> passwordActionCallback) {
            mPasswordActionCallback = passwordActionCallback;
        }

        @Override
        public void onClick(int buttonType) {
            switch (buttonType) {
                case ModalDialogView.ButtonType.POSITIVE:
                    mPasswordActionCallback.onResult(true);
                    break;
                case ModalDialogView.ButtonType.NEGATIVE:
                    mPasswordActionCallback.onResult(false);
                    break;
                default:
                    assert false : "Unexpected button pressed in dialog: " + buttonType;
            }
        }

        @Override
        public void onDismiss(@DialogDismissalCause int dismissalCause) {
            mPasswordActionCallback.onResult(false);
        }
    }

    public static void bind(
            PasswordGenerationDialogModel model, PasswordGenerationDialogViewHolder viewHolder) {
        viewHolder.setController(new PasswordGenerationDialogController(
                model.get(PasswordGenerationDialogModel.PASSWORD_ACTION_CALLBACK)));
        viewHolder.setGeneratedPassword(
                model.get(PasswordGenerationDialogModel.GENERATED_PASSWORD));
        viewHolder.setSaveExplanationText(
                model.get(PasswordGenerationDialogModel.SAVE_EXPLANATION_TEXT));
        viewHolder.initializeView();
    }
}
