// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.content.Context;
import android.graphics.drawable.Drawable;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omaha.UpdateStatusProvider;
import org.chromium.chrome.browser.password_manager.PasswordCheckReferrer;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelper;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridge;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.chrome.browser.safety_hub.SafetyHubModuleMediator.ModuleState;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.browser_ui.settings.SettingsCustomTabLauncher;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.function.Supplier;

/** Utility methods for common Safety Hub related actions. */
@NullMarked
class SafetyHubUtils {

    /**
     * Launches the Password Checkup UI from GMSCore.
     *
     * @param context used to show the dialog.
     */
    // TODO(crbug.com/388788969): Rename to `showAccountPasswordCheckUi`.
    static void showPasswordCheckUi(
            Context context,
            Profile profile,
            Supplier<@Nullable ModalDialogManager> modalDialogManagerSupplier,
            @Nullable SettingsCustomTabLauncher settingsCustomTabLauncher) {
        PasswordManagerHelper passwordManagerHelper = PasswordManagerHelper.getForProfile(profile);
        String account = getAccountEmail(profile);
        assert account != null
                : "The password check UI should only be launched for signed in Safety Hub users.";
        passwordManagerHelper.showPasswordCheckup(
                context,
                PasswordCheckReferrer.SAFETY_CHECK,
                modalDialogManagerSupplier,
                account,
                settingsCustomTabLauncher);
    }

    /**
     * Launches the Local Password Checkup UI from GMSCore.
     *
     * @param context used to show the dialog.
     */
    static void showLocalPasswordCheckUi(
            Context context,
            Profile profile,
            Supplier<@Nullable ModalDialogManager> modalDialogManagerSupplier,
            SettingsCustomTabLauncher settingsCustomTabLauncher) {
        PasswordManagerHelper passwordManagerHelper = PasswordManagerHelper.getForProfile(profile);
        passwordManagerHelper.showPasswordCheckup(
                context,
                PasswordCheckReferrer.SAFETY_CHECK,
                modalDialogManagerSupplier,
                /* accountEmail= */ null,
                settingsCustomTabLauncher);
    }

    /**
     * @return Whether the given {@link Profile} is currently signed in.
     */
    static boolean isSignedIn(Profile profile) {
        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(profile);
        assert identityManager != null;
        return identityManager.hasPrimaryAccount(ConsentLevel.SIGNIN);
    }

    /**
     * @return The email address associated with the currently signed in {@link Profile}.
     */
    static @Nullable String getAccountEmail(Profile profile) {
        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(profile);
        assert identityManager != null;
        return CoreAccountInfo.getEmailFrom(
                identityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN));
    }

    /**
     * @return The current safe browsing state.
     */
    static @SafeBrowsingState int getSafeBrowsingState(Profile profile) {
        return new SafeBrowsingBridge(profile).getSafeBrowsingState();
    }

    /**
     * @return Whether the Safe Browsing preference is managed.
     */
    static boolean isSafeBrowsingManaged(Profile profile) {
        return new SafeBrowsingBridge(profile).isSafeBrowsingManaged();
    }

    static @ModuleState int getUpdateCheckModuleState(
            UpdateStatusProvider.@Nullable UpdateStatus updateStatus) {
        if (updateStatus == null
                || updateStatus.updateState
                        == UpdateStatusProvider.UpdateState.UNSUPPORTED_OS_VERSION) {
            return ModuleState.UNAVAILABLE;
        }
        if (updateStatus.updateState == UpdateStatusProvider.UpdateState.UPDATE_AVAILABLE) {
            return ModuleState.WARNING;
        }
        return ModuleState.SAFE;
    }

    static @ModuleState int getPermissionsModuleState(int sitesWithUnusedPermissionsCount) {
        return (sitesWithUnusedPermissionsCount > 0) ? ModuleState.INFO : ModuleState.SAFE;
    }

    static @ModuleState int getNotificationModuleState(int notificationPermissionsForReviewCount) {
        return (notificationPermissionsForReviewCount > 0) ? ModuleState.INFO : ModuleState.SAFE;
    }

    static @ModuleState int getSafeBrowsingModuleState(@SafeBrowsingState int safeBrowsingState) {
        return (safeBrowsingState == SafeBrowsingState.NO_SAFE_BROWSING)
                ? ModuleState.WARNING
                : ModuleState.SAFE;
    }

    static Drawable getManagedIcon(Context context) {
        return SettingsUtils.getTintedIcon(
                context, R.drawable.ic_business, R.color.default_icon_color_secondary_tint_list);
    }
}
