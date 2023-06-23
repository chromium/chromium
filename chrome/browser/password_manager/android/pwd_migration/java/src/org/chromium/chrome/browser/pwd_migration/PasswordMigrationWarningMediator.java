// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_migration;

import static org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.ACCOUNT_DISPLAY_NAME;
import static org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.CURRENT_SCREEN;
import static org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.VISIBLE;

import androidx.fragment.app.FragmentManager;

import org.chromium.chrome.browser.password_manager.settings.PasswordListObserver;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.MigrationOption;
import org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.ScreenType;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
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
        mModel.set(VISIBLE, true);
        mModel.set(CURRENT_SCREEN, screenType);
        mModel.set(ACCOUNT_DISPLAY_NAME, getAccountDisplayName(mProfile));
    }

    void onDismissed(@StateChangeReason int reason) {
        if (!mModel.get(VISIBLE)) return; // Dismiss only if not dismissed yet.
        mModel.set(VISIBLE, false);
    }

    @Override
    public void onAcknowledge(BottomSheetController bottomSheetController) {
        mModel.set(VISIBLE, false);

        PrefService prefService = UserPrefs.get(mProfile);
        prefService.setBoolean(Pref.USER_ACKNOWLEDGED_LOCAL_PASSWORDS_MIGRATION_WARNING, true);
    }

    @Override
    public void onMoreOptions() {
        assert mModel.get(VISIBLE);
        mModel.set(CURRENT_SCREEN, ScreenType.OPTIONS_SCREEN);
    }

    @Override
    public void onNext(@MigrationOption int selectedOption, FragmentManager fragmentManager) {
        if (selectedOption == MigrationOption.SYNC_PASSWORDS) {
            mModel.set(VISIBLE, false);
            startSyncFlow();
        } else {
            mOptionsHandler.startExportFlow(fragmentManager);
        }
        // TODO(crbug.com/1445065): Launch the password Export flow.
    }

    @Override
    public void onCancel(BottomSheetController bottomSheetController) {
        mModel.set(VISIBLE, false);
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
        // TODO(crbug.com/1445065): Note down that the passwords are ready to try exporting.
    }

    @Override
    public void passwordExceptionListAvailable(int count) {
        // This is unused.
    }
}
