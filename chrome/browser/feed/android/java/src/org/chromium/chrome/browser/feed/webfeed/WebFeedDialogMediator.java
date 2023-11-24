// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.webfeed;

import android.view.View;

import org.chromium.base.Callback;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator class responsible for the logic of showing WebFeed dialogs. */
class WebFeedDialogMediator {
    private final ModalDialogManager mDialogManager;

    private PropertyModel mHostDialogModel;

    private class DialogClickHandler implements ModalDialogProperties.Controller {
        private final Callback<Integer> mCallback;

        DialogClickHandler(Callback<Integer> onClick) {
            mCallback = onClick;
        }

        @Override
        public void onClick(PropertyModel model, int buttonType) {
            switch (buttonType) {
                case ModalDialogProperties.ButtonType.POSITIVE:
                    dismissDialog(DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                    break;
                case ModalDialogProperties.ButtonType.NEGATIVE:
                    dismissDialog(DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                    break;
                default:
                    assert false : "Unexpected button pressed in dialog: " + buttonType;
            }
        }

        @Override
        public void onDismiss(PropertyModel model, @DialogDismissalCause int dismissalCause) {
            mCallback.onResult(dismissalCause);
        }
    }

    /**
     * Constructs an instance of {@link WebFeedDialogMediator}.
     *
     * @param dialogManager {@link ModalDialogManager} for managing the dialog.
     */
    WebFeedDialogMediator(ModalDialogManager dialogManager) {
        mDialogManager = dialogManager;
    }

    /**
     * Initializes the {@link WebFeedDialogMediator}.
     *
     * @param view The {@link View} to show.
     * @param dialogContents The {@link WebFeedDialogContents} containing the dialog contents.
     */
    void initialize(View view, WebFeedDialogContents dialogContents) {
        mHostDialogModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CUSTOM_VIEW, view)
                        .with(
                                ModalDialogProperties.CONTROLLER,
                                new DialogClickHandler(dialogContents.mButtonClickCallback))
                        .with(ModalDialogProperties.CONTENT_DESCRIPTION, dialogContents.mTitle)
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                dialogContents.mPrimaryButtonText)
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                dialogContents.mSecondaryButtonText)
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                dialogContents.mSecondaryButtonText != null
                                        ? ModalDialogProperties.ButtonStyles
                                                .PRIMARY_FILLED_NEGATIVE_OUTLINE
                                        : ModalDialogProperties.ButtonStyles
                                                .PRIMARY_OUTLINE_NEGATIVE_OUTLINE)
                        .build();
    }

    void showDialog() {
        mDialogManager.showDialog(mHostDialogModel, ModalDialogManager.ModalDialogType.APP);
    }

    void dismissDialog(int dismissalCause) {
        mDialogManager.dismissDialog(mHostDialogModel, dismissalCause);
    }
}
