// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.content.res.Resources;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator class responsible for initializing the model state. */
public class PasswordGenerationDialogMediator {
    private static class DialogController implements ModalDialogProperties.Controller {
        private final Callback<Boolean> mPasswordActionCallback;

        public DialogController(Callback<Boolean> passwordActionCallback) {
            mPasswordActionCallback = passwordActionCallback;
        }

        @Override
        public void onClick(PropertyModel model, int buttonType) {
            switch (buttonType) {
                case ModalDialogProperties.ButtonType.POSITIVE:
                    mPasswordActionCallback.onResult(true);
                    break;
                case ModalDialogProperties.ButtonType.NEGATIVE:
                    mPasswordActionCallback.onResult(false);
                    break;
                default:
                    assert false : "Unexpected button pressed in dialog: " + buttonType;
            }
        }

        @Override
        public void onDismiss(PropertyModel model, int dismissalCause) {
            mPasswordActionCallback.onResult(false);
        }
    }

    public static void initializeState(
            PasswordGenerationDialogModel model, String password, String saveExplanationText) {
        model.set(PasswordGenerationDialogModel.GENERATED_PASSWORD, password);
        model.set(PasswordGenerationDialogModel.SAVE_EXPLANATION_TEXT, saveExplanationText);
    }

    static PropertyModel.Builder createDialogModelBuilder(
            Callback<Boolean> onPasswordAcceptedOrRejected, View customView) {
        Resources resources = customView.getResources();
        PropertyModel.Builder builder =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER,
                                new DialogController(onPasswordAcceptedOrRejected))
                        .with(ModalDialogProperties.CUSTOM_VIEW, customView)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, resources,
                                R.string.password_generation_dialog_use_password_button)
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, resources,
                                R.string.password_generation_dialog_cancel_button);
        if (PasswordManagerHelper.usesUnifiedPasswordManagerBranding()) {
            builder = builder.with(ModalDialogProperties.TITLE_ICON, customView.getContext(),
                                     new PasswordManagerResourceProviderImpl()
                                             .getPasswordManagerIcon())
                              .with(ModalDialogProperties.TITLE, resources,
                                      R.string.password_generation_dialog_title_upm_branded);
        } else {
            builder = builder.with(ModalDialogProperties.TITLE, resources,
                    R.string.password_generation_dialog_title);
        }
        return builder;
    }
}