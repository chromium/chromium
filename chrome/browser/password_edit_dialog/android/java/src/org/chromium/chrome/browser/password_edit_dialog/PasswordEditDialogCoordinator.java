// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_edit_dialog;

import static org.chromium.chrome.browser.password_edit_dialog.PasswordEditDialogMediator.getTitle;
import static org.chromium.chrome.browser.password_edit_dialog.PasswordEditDialogMediator.isUpdate;

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
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Coordinator for password edit dialog. */
class PasswordEditDialogCoordinator {
    /**
     * A delegate interface for PasswordEditDialogBridge to receive the results of password edit
     * dialog interactions.
     */
    interface Delegate {
        /**
         * Called when the user taps the dialog positive button.
         * Used when PasswordEditDialogWithDetails feature is enabled.
         *
         * @param username The username, whose password is to be updated or saved (if it's new)
         * @param password The password to be saved
         */
        void onDialogAccepted(String username, String password);

        /**
         * Called when the user taps the dialog Continue button.
         * Used when PasswordEditDialogWithDetails feature is disabled.
         *
         * @param usernameIndex Selected username index from the list.
         */
        void onLegacyDialogAccepted(int usernameIndex);

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

    private PropertyModel mDialogModel;
    private PropertyModel mDialogViewModel;

    private PasswordEditDialogMediator mMediator;
    private boolean mIsDialogWithDetailsFeatureEnabled;

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
        mIsDialogWithDetailsFeatureEnabled =
                ChromeFeatureList.isEnabled(ChromeFeatureList.PASSWORD_EDIT_DIALOG_WITH_DETAILS);
        mMediator = new PasswordEditDialogMediator(
                mModalDialogManager, mContext.getResources(), delegate);
    }

    /**
     * Shows the dialog asking if user wants to save the password and providing
     * username & password editing capabilities.
     * Possible user choices: Save, Never for this site, Cancel
     * Called when PasswordEditDialogWithDetails feature flag is enabled.
     *
     * @param savedUsernames The list of usernames that are already saved in password manager
     *        for the current site.
     * @param username Initially typed username that user will be able to edit
     * @param password Initially typed password that user will be able to edit
     * @param account The account name where the password will be saved. When the user is not signed
     *         in the account is null.
     */
    void showPasswordEditDialog(@NonNull String[] savedUsernames, @NonNull String username,
            @NonNull String password, @Nullable String account) {
        List<String> savedUsernameList = Arrays.asList(savedUsernames);
        boolean update = isUpdate(savedUsernameList, username);
        // The Save password dialog has only user-entered username in the spinner's list.
        // The Update password dialog has all previously saved usernames.
        List<String> displayUsernamesList = update ? savedUsernameList : Arrays.asList(username);

        mDialogModel =
                createModalDialogModel(getTitle(savedUsernameList, displayUsernamesList, username),
                        update ? R.string.password_manager_update_button
                               : R.string.password_manager_save_button);
        mDialogViewModel = createDialogViewModel(displayUsernamesList, username, password, account);

        mMediator.initialize(mDialogViewModel, mDialogModel, Arrays.asList(savedUsernames));
        // The mediator needs to be initialized before the model change processor,
        // so that the callbacks handling changes from the view are not null
        // when the view is populated.
        PropertyModelChangeProcessor.create(
                mDialogViewModel, mDialogView, PasswordEditDialogViewBinder::bind);

        mModalDialogManager.showDialog(mDialogModel,
                mIsDialogWithDetailsFeatureEnabled ? ModalDialogManager.ModalDialogType.APP
                                                   : ModalDialogManager.ModalDialogType.TAB);
    }

    /**
     * Shows the dialog asking if user wants to update the password and providing
     * username selection capabilities.
     * Called when PasswordEditDialogWithDetails feature flag is disabled.
     *
     * @param usernames The list of usernames that will be presented in the Spinner.
     * @param selectedUsernameIndex The index in the usernames list of the user that should be
     *         selected initially.
     * @param account The account name where the password will be saved. When the user is not signed
     *         in the account is null.
     */
    void showLegacyPasswordEditDialog(
            @NonNull String[] usernames, int selectedUsernameIndex, @Nullable String account) {
        List<String> savedUsernameList = Arrays.asList(usernames);
        mDialogModel = createModalDialogModel(
                R.string.confirm_username_dialog_title, R.string.password_manager_update_button);
        mDialogViewModel = createLegacyDialogViewModel(savedUsernameList, selectedUsernameIndex);

        mMediator.initialize(mDialogViewModel, mDialogModel, savedUsernameList);
        // The mediator needs to be initialized before the model change processor,
        // so that the callbacks handling changes from the view are not null
        // when the view is populated.
        PropertyModelChangeProcessor.create(
                mDialogViewModel, mDialogView, PasswordEditDialogViewBinder::bind);

        mModalDialogManager.showDialog(mDialogModel, ModalDialogManager.ModalDialogType.TAB);
    }

    private PropertyModel createDialogViewModel(
            List<String> displayedUsernames, String username, String password, String account) {
        return new PropertyModel.Builder(PasswordEditDialogProperties.ALL_KEYS)
                .with(PasswordEditDialogProperties.USERNAMES,
                        removeEmptyStrings(displayedUsernames))
                .with(PasswordEditDialogProperties.PASSWORD, password)
                .with(PasswordEditDialogProperties.FOOTER,
                        mContext.getString(getEditPasswordDialogFooterId(account), account))
                .with(PasswordEditDialogProperties.USERNAME, username)
                .with(PasswordEditDialogProperties.USERNAME_CHANGED_CALLBACK,
                        mMediator::handleUsernameChanged)
                .with(PasswordEditDialogProperties.PASSWORD_CHANGED_CALLBACK,
                        mMediator::handlePasswordChanged)
                .build();
    }

    private PropertyModel createLegacyDialogViewModel(
            List<String> displayedUsernames, int selectedUsernameIndex) {
        return new PropertyModel.Builder(PasswordEditDialogProperties.ALL_KEYS)
                .with(PasswordEditDialogProperties.USERNAMES, displayedUsernames)
                .with(PasswordEditDialogProperties.USERNAME_INDEX, selectedUsernameIndex)
                .with(PasswordEditDialogProperties.USERNAME_SELECTED_CALLBACK,
                        mMediator::handleUsernameSelected)
                .build();
    }

    private PropertyModel createModalDialogModel(
            @StringRes int title, @StringRes int positiveButtonText) {
        Resources resources = mContext.getResources();
        PasswordManagerResourceProvider resourceProvider =
                PasswordManagerResourceProviderFactory.create();
        PropertyModel.Builder dialogModeBuilder =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, mMediator)
                        .with(ModalDialogProperties.TITLE, resources, title)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, resources,
                                positiveButtonText)
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, resources,
                                R.string.password_generation_dialog_cancel_button)
                        .with(ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                        .with(ModalDialogProperties.CUSTOM_VIEW, mDialogView);
        if (mIsDialogWithDetailsFeatureEnabled) {
            dialogModeBuilder.with(ModalDialogProperties.TITLE_ICON, mContext,
                    resourceProvider.getPasswordManagerIcon());
        }
        return dialogModeBuilder.build();
    }

    /** Dismisses the displayed dialog. */
    void dismiss() {
        mModalDialogManager.dismissDialog(mDialogModel, DialogDismissalCause.DISMISSED_BY_NATIVE);
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

    private static List<String> removeEmptyStrings(List<String> strings) {
        List<String> nonEmptyStrings = new ArrayList<>();
        for (String str : strings) {
            if (!str.isEmpty()) nonEmptyStrings.add(str);
        }
        return nonEmptyStrings;
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