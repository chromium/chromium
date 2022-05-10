// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_edit_dialog;

import android.content.Context;
import android.content.res.Resources;
import android.text.TextUtils;
import android.view.LayoutInflater;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.password_manager.PasswordManagerResourceProvider;
import org.chromium.chrome.browser.password_manager.PasswordManagerResourceProviderFactory;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.Arrays;

/** Coordinator for password edit dialog. */
class PasswordEditDialogCoordinator implements ModalDialogProperties.Controller {
    /**
     * A delegate interface for PasswordEditDialogBridge to receive the results of password edit
     * dialog interactions.
     */
    interface Delegate {
        /**
         * Called when the user taps the dialog positive button.
         *
         * @param username The username, whose password is to be updated or saved (if it's new)
         * @param password The password to be saved
         */
        void onDialogAccepted(String username, String password);

        /**
         * Called when the dialog is dismissed.
         *
         * @param dialogAccepted Indicates whether the dialog was accepted or cancelled by the user.
         */
        void onDialogDismissed(boolean dialogAccepted);
    }

    private final Context mContext;
    private final ModalDialogManager mModalDialogManager;
    private final PasswordEditDialogView mDialogView;
    private final Delegate mDelegate;

    private PropertyModel mDialogModel;
    private PropertyModel mDialogViewModel;

    /**
     * Creates the {@link PasswordEditDialogCoordinator}.
     *
     * @param windowAndroid The window where the dialog will be displayed.
     * @param delegate The delegate to be called with results of interaction.
     */
    static PasswordEditDialogCoordinator create(
            @NonNull WindowAndroid windowAndroid, @NonNull Delegate delegate) {
        Context context = windowAndroid.getContext().get();

        return new PasswordEditDialogCoordinator(context, windowAndroid.getModalDialogManager(),
                createPasswordEditDialogView(context), delegate);
    }

    private static PasswordEditDialogView createPasswordEditDialogView(Context context) {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.PASSWORD_EDIT_DIALOG_WITH_DETAILS)
                ? (PasswordEditDialogWithDetailsView) LayoutInflater.from(context).inflate(
                        R.layout.password_edit_dialog_with_details, null)
                : (UsernameSelectionConfirmationView) LayoutInflater.from(context).inflate(
                        R.layout.password_edit_dialog, null);
    }

    /**
     * Internal constructor for {@link PasswordEditDialogCoordinator}. Used by tests to inject
     * parameters. External code should use PasswordEditDialogCoordinator#create.
     *
     * @param context The context for accessing resources.
     * @param modalDialogManager The ModalDialogManager to display the dialog.
     * @param dialogView The custom view with dialog content.
     * @param delegate The delegate to be called with results of interaction.
     */
    @VisibleForTesting
    PasswordEditDialogCoordinator(@NonNull Context context,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull PasswordEditDialogView dialogView, @NonNull Delegate delegate) {
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mDialogView = dialogView;
        mDelegate = delegate;
    }

    /**
     * Shows the password edit dialog.
     *
     * @param usernames The list of usernames that will be presented in the Spinner.
     * @param selectedUsernameIndex The index in the usernames list of the user that should be
     *         selected initially.
     * @param password The password.
     * @param origin The origin with which these credentials are associated.
     * @param account The account name where the password will be saved. When the user is not signed
     *         in the account is null.
     */
    void show(@NonNull String[] usernames, int selectedUsernameIndex, @NonNull String password,
            @NonNull String origin, @Nullable String account) {
        createDialogViewModel(usernames, selectedUsernameIndex, password, account);
        PropertyModelChangeProcessor.create(
                mDialogViewModel, mDialogView, PasswordEditDialogViewBinder::bind);

        createModelDialogModel();
        mModalDialogManager.showDialog(mDialogModel, ModalDialogManager.ModalDialogType.TAB);
    }

    private void createDialogViewModel(
            String[] usernames, int selectedUsernameIndex, String password, String account) {
        Resources resources = mContext.getResources();
        PropertyModel.Builder dialogViewModelBuilder =
                new PropertyModel.Builder(PasswordEditDialogProperties.ALL_KEYS)
                        .with(PasswordEditDialogProperties.USERNAMES, Arrays.asList(usernames))
                        .with(PasswordEditDialogProperties.USERNAME,
                                usernames[selectedUsernameIndex])
                        .with(PasswordEditDialogProperties.USERNAME_CHANGED_CALLBACK,
                                this::handleUsernameChanged)
                        .with(PasswordEditDialogProperties.PASSWORD, password);
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.PASSWORD_EDIT_DIALOG_WITH_DETAILS)) {
            dialogViewModelBuilder
                    .with(PasswordEditDialogProperties.FOOTER,
                            resources.getString(getEditPasswordDialogFooterId(account), account))
                    .with(PasswordEditDialogProperties.PASSWORD_CHANGED_CALLBACK,
                            this::handlePasswordChanged)
                    .build();
        }
        mDialogViewModel = dialogViewModelBuilder.build();
    }

    private void createModelDialogModel() {
        Resources resources = mContext.getResources();
        PasswordManagerResourceProvider resourceProvider =
                PasswordManagerResourceProviderFactory.create();
        PropertyModel.Builder dialogModeBuilder =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, this)
                        // TODO(crbug.com/1237077): Currently PasswordEditDialog is only
                        // used for confirming username in update password flow. The
                        // positive button text is set to "Update". In the future, when
                        // this dialog is used in other scenarios, the buttontext should
                        // be set dynamically based on scenario.
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, resources,
                                R.string.password_manager_update_button)
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, resources,
                                R.string.password_generation_dialog_cancel_button)
                        .with(ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                        .with(ModalDialogProperties.CUSTOM_VIEW, mDialogView);
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.PASSWORD_EDIT_DIALOG_WITH_DETAILS)) {
            dialogModeBuilder
                    .with(ModalDialogProperties.TITLE, resources,
                            R.string.password_update_dialog_title)
                    .with(ModalDialogProperties.TITLE_ICON, mContext,
                            resourceProvider.getPasswordManagerIcon());
        } else {
            dialogModeBuilder.with(
                    ModalDialogProperties.TITLE, resources, R.string.confirm_username_dialog_title);
        }
        mDialogModel = dialogModeBuilder.build();
    }

    /** Dismisses the displayed dialog. */
    void dismiss() {
        mModalDialogManager.dismissDialog(mDialogModel, DialogDismissalCause.DISMISSED_BY_NATIVE);
    }

    private void handleUsernameChanged(String selectedUsername) {
        mDialogViewModel.set(PasswordEditDialogProperties.USERNAME, selectedUsername);
    }

    private void handlePasswordChanged(String password) {
        mDialogViewModel.set(PasswordEditDialogProperties.PASSWORD, password);
        mDialogViewModel.set(PasswordEditDialogProperties.EMPTY_PASSWORD_ERROR, password.isEmpty());
    }

    // ModalDialogProperties.Controller implementation.
    @Override
    public void onClick(PropertyModel model, @ButtonType int buttonType) {
        if (buttonType == ButtonType.POSITIVE) {
            mDelegate.onDialogAccepted(mDialogViewModel.get(PasswordEditDialogProperties.USERNAME),
                    mDialogViewModel.get(PasswordEditDialogProperties.PASSWORD));
        }
        mModalDialogManager.dismissDialog(model,
                buttonType == ButtonType.POSITIVE ? DialogDismissalCause.POSITIVE_BUTTON_CLICKED
                                                  : DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
    }

    @Override
    public void onDismiss(PropertyModel model, @DialogDismissalCause int dismissalCause) {
        mDelegate.onDialogDismissed(dismissalCause == DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
    }

    private @StringRes int getEditPasswordDialogFooterId(String account) {
        if (TextUtils.isEmpty(account)) {
            return BuildConfig.IS_CHROME_BRANDED
                    ? R.string.password_edit_dialog_unsynced_footer_google
                    : R.string.password_edit_dialog_unsynced_footer;
        } else {
            return R.string.password_edit_dialog_synced_footer_google;
        }
    }

    @VisibleForTesting
    PropertyModel getDialogModelForTesting() {
        return mDialogModel;
    }

    @VisibleForTesting
    PropertyModel getDialogViewModelForTesting() {
        return mDialogViewModel;
    }
}