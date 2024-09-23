// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_migration;

import static org.chromium.chrome.browser.password_manager.PasswordMetricsUtil.logPasswordMigrationWarningUserAction;
import static org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.ACCOUNT_DISPLAY_NAME;
import static org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.CURRENT_SCREEN;
import static org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.SHOULD_OFFER_SYNC;
import static org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.VISIBLE;

import android.net.Uri;

import androidx.annotation.Nullable;
import androidx.fragment.app.FragmentManager;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.password_manager.PasswordManagerBuildflags;
import org.chromium.chrome.browser.password_manager.PasswordMetricsUtil;
import org.chromium.chrome.browser.password_manager.PasswordMetricsUtil.PasswordMigrationWarningSheetStateAtClosing;
import org.chromium.chrome.browser.password_manager.PasswordMetricsUtil.PasswordMigrationWarningUserActions;
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

/**
 * Contains the logic for the local passwords migration warning. It sets the state of the model and
 * reacts to events.
 */
class PasswordMigrationWarningMediator
        implements PasswordMigrationWarningOnClickHandler, PasswordListObserver {
    private PropertyModel mModel;
    private Profile mProfile;
    private MigrationWarningOptionsHandler mOptionsHandler;
    private @PasswordMigrationWarningTriggers int mReferrer;

    public interface MigrationWarningOptionsHandler {
        /** Launches the sync consent flow. */
        void startSyncConsentFlow();

        /** Opens the sync settings to allow users to enable passwords sync. */
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

        /** Resumes the password export flow. */
        void resumeExportFlow();

        /** Notifies the {@link ExportFlow} that passwords are fetched. */
        void passwordsAvailable();
    }

    PasswordMigrationWarningMediator(
            Profile profile,
            MigrationWarningOptionsHandler optionsHandler,
            @PasswordMigrationWarningTriggers int referrer) {
        mProfile = profile;
        mOptionsHandler = optionsHandler;
        mReferrer = referrer;
    }

    void initializeModel(PropertyModel model) {
        mModel = model;
    }

    void showWarning(@ScreenType int screenType) {
        mModel.set(SHOULD_OFFER_SYNC, shouldOfferSync());
        mModel.set(VISIBLE, true);
        mModel.set(CURRENT_SCREEN, screenType);
        mModel.set(ACCOUNT_DISPLAY_NAME, getAccountDisplayName(mProfile));
    }

    void onShown() {
        if (mReferrer != PasswordMigrationWarningTriggers.CHROME_STARTUP) {
            return;
        }
        PrefService prefService = UserPrefs.get(mProfile);
        prefService.setBoolean(Pref.LOCAL_PASSWORD_MIGRATION_WARNING_SHOWN_AT_STARTUP, true);
    }

    /**
     * Called when BottomSheetObserver.onSheetClosed is invoked.
     * This is not the same as onDismissed, which is called in both onSheetClosed and in
     * onSheetStateChanged. This is because sometimes, if the BottomSheet was interrupted,
     * onSheetClosed won't get called.
     *
     * @param reason is the cause for the sheet to change its state.
     * @param setFragmentWasCalled indicates that the PasswordMigrationWarningView.setFragment()
     *         method was called.
     */
    void onSheetClosed(
            @BottomSheetController.StateChangeReason int reason, boolean setFragmentWasCalled) {
        if (!setFragmentWasCalled) {
            recordEmptySheetTrigger(mReferrer);
            resetTimestamp();
        }
        recordSheetStateAtClosing(reason, setFragmentWasCalled);
    }

    void onDismissed(@StateChangeReason int reason) {
        if (!mModel.get(VISIBLE)) return; // Dismiss only if not dismissed yet.
        mModel.set(VISIBLE, false);

        if (reason == StateChangeReason.SWIPE
                || reason == StateChangeReason.BACK_PRESS
                || reason == StateChangeReason.TAP_SCRIM
                || reason == StateChangeReason.OMNIBOX_FOCUS) {
            int dismissalKind =
                    mModel.get(CURRENT_SCREEN) == ScreenType.INTRO_SCREEN
                            ? PasswordMigrationWarningUserActions.DISMISS_INTRODUCTION
                            : PasswordMigrationWarningUserActions.DISMISS_MORE_OPTIONS;

            logPasswordMigrationWarningUserAction(dismissalKind);
        }
    }

    @Override
    public void onAcknowledge(BottomSheetController bottomSheetController) {
        mModel.set(VISIBLE, false);

        PrefService prefService = UserPrefs.get(mProfile);
        prefService.setBoolean(Pref.USER_ACKNOWLEDGED_LOCAL_PASSWORDS_MIGRATION_WARNING, true);

        logPasswordMigrationWarningUserAction(PasswordMigrationWarningUserActions.GOT_IT);
    }

    @Override
    public void onMoreOptions() {
        assert mModel.get(VISIBLE);
        mModel.set(CURRENT_SCREEN, ScreenType.OPTIONS_SCREEN);

        logPasswordMigrationWarningUserAction(PasswordMigrationWarningUserActions.MORE_OPTIONS);
    }

    @Override
    public void onNext(@MigrationOption int selectedOption, FragmentManager fragmentManager) {
        if (selectedOption == MigrationOption.SYNC_PASSWORDS) {
            mModel.set(VISIBLE, false);
            startSyncFlow();

            logPasswordMigrationWarningUserAction(PasswordMigrationWarningUserActions.SYNC);
        } else {
            mOptionsHandler.startExportFlow(fragmentManager);

            logPasswordMigrationWarningUserAction(PasswordMigrationWarningUserActions.EXPORT);
        }
    }

    @Override
    public void onSavePasswordsToDownloads(Uri passwordsFile) {
        mOptionsHandler.savePasswordsToDownloads(passwordsFile);
    }

    @Override
    public void onCancel(BottomSheetController bottomSheetController) {
        mModel.set(VISIBLE, false);

        logPasswordMigrationWarningUserAction(PasswordMigrationWarningUserActions.CANCEL);
    }

    private @Nullable String getAccountDisplayName(Profile profile) {
        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(profile);
        CoreAccountInfo coreAccountInfo =
                identityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        if (coreAccountInfo == null || coreAccountInfo.getEmail().isEmpty()) {
            return null;
        }
        @Nullable
        AccountInfo account =
                identityManager.findExtendedAccountInfoByEmailAddress(coreAccountInfo.getEmail());
        if (account == null) {
            return coreAccountInfo.getEmail();
        }
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
        if (!PasswordManagerBuildflags.USE_LOGIN_DATABASE_AS_BACKEND) {
            return false;
        }
        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(mProfile);
        if (signinManager == null || signinManager.isSigninDisabledByPolicy()) {
            return false;
        }
        SyncService syncService = SyncServiceFactory.getForProfile(mProfile);
        if (syncService == null) {
            return false;
        }
        if (syncService.isSyncDisabledByEnterprisePolicy()) {
            return false;
        }
        if (syncService.isTypeManagedByPolicy(UserSelectableType.PASSWORDS)) {
            return false;
        }
        return true;
    }

    private void resetTimestamp() {
        PrefService prefService = UserPrefs.get(mProfile);
        prefService.setString(Pref.LOCAL_PASSWORDS_MIGRATION_WARNING_SHOWN_TIMESTAMP, "0");
    }

    private void recordEmptySheetTrigger(@PasswordMigrationWarningTriggers int referrer) {
        RecordHistogram.recordEnumeratedHistogram(
                PasswordMetricsUtil.PASSWORD_MIGRATION_WARNING_EMPTY_SHEET_TRIGGER,
                referrer,
                PasswordMigrationWarningTriggers.MAX_VALUE);
    }

    private void recordSheetStateAtClosing(
            @BottomSheetController.StateChangeReason int reason, boolean setFragmentWasCalled) {
        RecordHistogram.recordEnumeratedHistogram(
                PasswordMetricsUtil.PASSWORD_MIGRATION_WARNING_SHEET_STATE_AT_CLOSING,
                getSheetStateAtClosingBucket(reason, setFragmentWasCalled),
                PasswordMigrationWarningSheetStateAtClosing.COUNT);
    }

    @PasswordMigrationWarningSheetStateAtClosing
    private int getSheetStateAtClosingBucket(
            @BottomSheetController.StateChangeReason int reason, boolean setFragmentWasCalled) {
        if (setFragmentWasCalled) {
            return PasswordMigrationWarningSheetStateAtClosing.FULL_SHEET_CLOSED;
        }
        switch (reason) {
            case StateChangeReason.SWIPE:
            case StateChangeReason.BACK_PRESS:
            case StateChangeReason.TAP_SCRIM:
            case StateChangeReason.NAVIGATION:
            case StateChangeReason.OMNIBOX_FOCUS:
                return PasswordMigrationWarningSheetStateAtClosing
                        .EMPTY_SHEET_CLOSED_BY_USER_INTERACTION;
            default:
                return PasswordMigrationWarningSheetStateAtClosing
                        .EMPTY_SHEET_CLOSED_WITHOUT_USER_INTERACTION;
        }
    }
}
