// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.add_username_dialog;

import static org.chromium.chrome.browser.add_username_dialog.AddUsernameDialogContentProperties.PASSWORD;
import static org.chromium.chrome.browser.add_username_dialog.AddUsernameDialogContentProperties.USERNAME;
import static org.chromium.chrome.browser.add_username_dialog.AddUsernameDialogContentProperties.USERNAME_CHANGED_CALLBACK;

import android.content.Context;
import android.content.res.Resources;
import android.view.LayoutInflater;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.password_manager.PasswordManagerResourceProvider;
import org.chromium.chrome.browser.password_manager.PasswordManagerResourceProviderFactory;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator for the add username dialog. The dialog is displayed when the password is being
 * automatically saved and the username could not be captured from the password form. It allows user
 * to add the username for the saved password.
 */
public class AddUsernameDialogController implements ModalDialogProperties.Controller {
    /** A delegate interface to receive the results of the dialog interaction. */
    interface Delegate {
        /**
         * Called when the user taps the dialog positive button.
         *
         * @param username The username that will be saved in the password store.
         */
        void onDialogAccepted(String username);

        /** Called when the dialog is dismissed. */
        void onDialogDismissed();
    }

    private final Context mContext;
    private final ModalDialogManager mModalDialogManager;
    private final Delegate mDelegate;
    private PropertyModel mModalDialogModel;
    private final AddUsernameDialogContentView mContentView;
    private PropertyModel mContentViewModel;

    /**
     * Creates the {@link AddUsernameDialogController}.
     *
     * @param context The context for accessing resources.
     * @param modalDialogManager The manager for displaying the modal dialog.
     * @param delegate The delegate to be called with the results of the interaction.
     */
    public AddUsernameDialogController(
            Context context, ModalDialogManager modalDialogManager, @NonNull Delegate delegate) {
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mDelegate = delegate;
        mContentView =
                (AddUsernameDialogContentView)
                        LayoutInflater.from(context)
                                .inflate(R.layout.add_username_dialog_content, null);
    }

    public void showAddUsernameDialog(String password) {
        mContentViewModel = createContentViewModel(password);
        PropertyModelChangeProcessor.create(
                mContentViewModel, mContentView, AddUsernameDialogContentViewBinder::bind);

        mModalDialogModel = createModalDialogModel();
        mModalDialogManager.showDialog(mModalDialogModel, ModalDialogManager.ModalDialogType.APP);
    }

    private PropertyModel createContentViewModel(String password) {
        return new PropertyModel.Builder(AddUsernameDialogContentProperties.ALL_KEYS)
                .with(PASSWORD, password)
                .with(USERNAME, "")
                .with(USERNAME_CHANGED_CALLBACK, this::onUsernameChanged)
                .build();
    }

    private PropertyModel createModalDialogModel() {
        Resources resources = mContext.getResources();
        PasswordManagerResourceProvider resourceProvider =
                PasswordManagerResourceProviderFactory.create();
        PropertyModel.Builder dialogModeBuilder =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, this)
                        .with(ModalDialogProperties.CUSTOM_VIEW, mContentView)
                        .with(
                                ModalDialogProperties.TITLE,
                                resources,
                                R.string.add_username_dialog_title)
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                resources,
                                R.string.add_username_dialog_add_username)
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                resources,
                                R.string.add_username_dialog_cancel)
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                        .with(
                                ModalDialogProperties.TITLE_ICON,
                                mContext,
                                resourceProvider.getPasswordManagerIcon());
        return dialogModeBuilder.build();
    }

    public void dismissDialog() {
        mModalDialogManager.dismissDialog(
                mModalDialogModel, DialogDismissalCause.DISMISSED_BY_NATIVE);
    }

    private void onUsernameChanged(String username) {
        mContentViewModel.set(AddUsernameDialogContentProperties.USERNAME, username);
    }

    // ModalDialogProperties.Controller implementation.
    @Override
    public void onClick(PropertyModel model, @ButtonType int buttonType) {
        if (buttonType == ButtonType.POSITIVE) {
            mDelegate.onDialogAccepted(
                    mContentViewModel.get(AddUsernameDialogContentProperties.USERNAME));
        }
        mModalDialogManager.dismissDialog(
                model,
                buttonType == ButtonType.POSITIVE
                        ? DialogDismissalCause.POSITIVE_BUTTON_CLICKED
                        : DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
    }

    @Override
    public void onDismiss(PropertyModel model, @DialogDismissalCause int dismissalCause) {
        mDelegate.onDialogDismissed();
    }
}
