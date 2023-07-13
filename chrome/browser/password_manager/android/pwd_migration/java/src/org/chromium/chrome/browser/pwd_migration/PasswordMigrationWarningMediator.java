// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_migration;

import static org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.ACCOUNT_DISPLAY_NAME;
import static org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.CURRENT_SCREEN;
import static org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.SHOULD_OFFER_SYNC;
import static org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.VISIBLE;

import android.net.Uri;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;
import androidx.fragment.app.FragmentManager;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.password_manager.settings.PasswordListObserver;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.MigrationOption;
import org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.ScreenType;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.Tribool;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Contains the logic for the local passwords migration warning. It sets the state of the model and
 * reacts to events.
 */
class PasswordMigrationWarningMediator
        implements PasswordMigrationWarningOnClickHandler, PasswordListObserver {
    /**
     * The action users take on the password migration warning sheet.
     *
     * Entries should not be renumbered and numeric values should never be reused. Needs to stay
     * in sync with PasswordMigrationWarningUserActions in enums.xml.
     */
    @IntDef({PasswordMigrationWarningUserActions.GOT_IT,
            PasswordMigrationWarningUserActions.MORE_OPTIONS,
            PasswordMigrationWarningUserActions.SYNC, PasswordMigrationWarningUserActions.EXPORT,
            PasswordMigrationWarningUserActions.CANCEL,
            PasswordMigrationWarningUserActions.DISMISS_INTRODUCTION,
            PasswordMigrationWarningUserActions.DISMISS_MORE_OPTIONS,
            PasswordMigrationWarningUserActions.COUNT})
    @Retention(RetentionPolicy.SOURCE)
    @interface PasswordMigrationWarningUserActions {
        int GOT_IT = 0;
        int MORE_OPTIONS = 1;
        int SYNC = 2;
        int EXPORT = 3;
        int CANCEL = 4;
        int DISMISS_INTRODUCTION = 5;
        int DISMISS_MORE_OPTIONS = 6;
        int COUNT = 7;
    }
    @VisibleForTesting
    static final String PASSWORD_MIGRATION_WARNING_USER_ACTIONS =
            "PasswordManager.PasswordMigrationWarning.UserAction";
    private PropertyModel mModel;
    private Profile mProfile;
    private MigrationWarningOptionsHandler mOptionsHandler;

    public interface MigrationWarningOptionsHandler {
        /**
         * Launches the sync consent flow.
         */
        void startSyncConsentFlow();

        /**
         * Opens the sync settings to allow users to enable passwords sync.
         */
        void openSyncSettings();

        /**
         * Launches the password export flow.
         *
         * @param fragmentManager for the fragment that owns the export flow.
         */
        void startExportFlow(FragmentManager fragmentManager);

        /**
         * Writes the passwords into the file.
         *
         * @param passwordsFile The file into which the passwords are expected to be saved.
         */
        void savePasswordsToDownloads(Uri passwordsFile);

        /**
         * Resumes the password export flow.
         */
        void resumeExportFlow();

        /**
         * Notifies the {@link ExportFlow} that passwords are fetched.
         */
        void passwordsAvailable();
    }

    PasswordMigrationWarningMediator(
            Profile profile, MigrationWarningOptionsHandler optionsHandler) {
        mProfile = profile;
        mOptionsHandler = optionsHandler;
    }

    void initializeModel(PropertyModel model) {
        mModel = model;
    }

    void showWarning(int screenType) {
        mModel.set(SHOULD_OFFER_SYNC, shouldOfferSync());
        mModel.set(VISIBLE, true);
        mModel.set(CURRENT_SCREEN, screenType);
        mModel.set(ACCOUNT_DISPLAY_NAME, getAccountDisplayName(mProfile));
    }

    void onDismissed(@StateChangeReason int reason) {
        if (!mModel.get(VISIBLE)) return; // Dismiss only if not dismissed yet.
        mModel.set(VISIBLE, false);

        if (reason == StateChangeReason.SWIPE || reason == StateChangeReason.BACK_PRESS
                || reason == StateChangeReason.TAP_SCRIM
                || reason == StateChangeReason.OMNIBOX_FOCUS) {
            RecordHistogram.recordEnumeratedHistogram(PASSWORD_MIGRATION_WARNING_USER_ACTIONS,
                    mModel.get(CURRENT_SCREEN) == ScreenType.INTRO_SCREEN
                            ? PasswordMigrationWarningUserActions.DISMISS_INTRODUCTION
                            : PasswordMigrationWarningUserActions.DISMISS_MORE_OPTIONS,
                    PasswordMigrationWarningUserActions.COUNT);
        }
    }

    @Override
    public void onAcknowledge(BottomSheetController bottomSheetController) {
        mModel.set(VISIBLE, false);

        PrefService prefService = UserPrefs.get(mProfile);
        prefService.setBoolean(Pref.USER_ACKNOWLEDGED_LOCAL_PASSWORDS_MIGRATION_WARNING, true);

        RecordHistogram.recordEnumeratedHistogram(PASSWORD_MIGRATION_WARNING_USER_ACTIONS,
                PasswordMigrationWarningUserActions.GOT_IT,
                PasswordMigrationWarningUserActions.COUNT);
    }

    @Override
    public void onMoreOptions() {
        assert mModel.get(VISIBLE);
        mModel.set(CURRENT_SCREEN, ScreenType.OPTIONS_SCREEN);

        RecordHistogram.recordEnumeratedHistogram(PASSWORD_MIGRATION_WARNING_USER_ACTIONS,
                PasswordMigrationWarningUserActions.MORE_OPTIONS,
                PasswordMigrationWarningUserActions.COUNT);
    }

    @Override
    public void onNext(@MigrationOption int selectedOption, FragmentManager fragmentManager) {
        if (selectedOption == MigrationOption.SYNC_PASSWORDS) {
            mModel.set(VISIBLE, false);
            startSyncFlow();

            RecordHistogram.recordEnumeratedHistogram(PASSWORD_MIGRATION_WARNING_USER_ACTIONS,
                    PasswordMigrationWarningUserActions.SYNC,
                    PasswordMigrationWarningUserActions.COUNT);
        } else {
            mOptionsHandler.startExportFlow(fragmentManager);

            RecordHistogram.recordEnumeratedHistogram(PASSWORD_MIGRATION_WARNING_USER_ACTIONS,
                    PasswordMigrationWarningUserActions.EXPORT,
                    PasswordMigrationWarningUserActions.COUNT);
        }
    }

    @Override
    public void onSavePasswordsToDownloads(Uri passwordsFile) {
        mOptionsHandler.savePasswordsToDownloads(passwordsFile);
    }

    @Override
    public void onCancel(BottomSheetController bottomSheetController) {
        mModel.set(VISIBLE, false);

        RecordHistogram.recordEnumeratedHistogram(PASSWORD_MIGRATION_WARNING_USER_ACTIONS,
                PasswordMigrationWarningUserActions.CANCEL,
                PasswordMigrationWarningUserActions.COUNT);
    }

    private String getAccountDisplayName(Profile profile) {
        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(profile);
        CoreAccountInfo coreAccountInfo =
                identityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        if (coreAccountInfo == null) {
            return null;
        }
        AccountInfo account =
                identityManager.findExtendedAccountInfoByEmailAddress(coreAccountInfo.getEmail());
        boolean canHaveEmailAddressDisplayed =
                account.getAccountCapabilities().canHaveEmailAddressDisplayed() != Tribool.FALSE;
        return canHaveEmailAddressDisplayed ? account.getEmail() : account.getFullName();
    }

    private void startSyncFlow() {
        SyncService syncService = SyncServiceFactory.getForProfile(mProfile);
        if (syncService == null) return;
        if (!syncService.isSyncFeatureEnabled()) {
            mOptionsHandler.startSyncConsentFlow();
            return;
        }
        assert !syncService.getSelectedTypes().contains(UserSelectableType.PASSWORDS);
        mOptionsHandler.openSyncSettings();
    }

    @Override
    public void passwordListAvailable(int count) {
        mOptionsHandler.passwordsAvailable();
    }

    @Override
    public void passwordExceptionListAvailable(int count) {
        // This is unused.
    }

    private boolean shouldOfferSync() {
        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(mProfile);
        if (signinManager == null || signinManager.isSigninDisabledByPolicy()) return false;

        SyncService syncService = SyncServiceFactory.getForProfile(mProfile);
        if (syncService == null) return false;
        if (syncService.isSyncDisabledByEnterprisePolicy()) return false;
        if (syncService.isTypeManagedByPolicy(UserSelectableType.PASSWORDS)) return false;
        return true;
    }
}
