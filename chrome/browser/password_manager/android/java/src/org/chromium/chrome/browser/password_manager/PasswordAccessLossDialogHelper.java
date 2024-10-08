// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.content.Context;

import androidx.fragment.app.FragmentActivity;

import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.access_loss.PasswordAccessLossDialogSettingsCoordinator;
import org.chromium.chrome.browser.access_loss.PasswordAccessLossPostExportDialogController;
import org.chromium.chrome.browser.access_loss.PasswordAccessLossWarningType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.password_manager.settings.PasswordAccessLossExportFlowCoordinator;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * Contains all the logic for showing password access loss dialog when trying to access password
 * settings.
 */
public class PasswordAccessLossDialogHelper {

    /**
     * Shows the modal dialog warning the user that they'd loose access to their passwords (only
     * when GMS Core is not functioning correctly).
     *
     * @param profile Chrome profile.
     * @param context used to provide resources and start intents from the dialog.
     * @param referrer indicates where the request to show the password settings UI comes from.
     * @param modalDialogManagerSupplier displays the dialog.
     * @param customTabIntentHelper needed to show help.
     * @param buildInfo needed to extract GMS Core version.
     * @return whether the dialog was displayed or not.
     */
    public static boolean tryShowAccessLossWarning(
            Profile profile,
            Context context,
            @ManagePasswordsReferrer int referrer,
            Supplier<ModalDialogManager> modalDialogManagerSupplier,
            CustomTabIntentHelper customTabIntentHelper,
            BuildInfo buildInfo) {
        PrefService prefService = UserPrefs.get(profile);
        @PasswordAccessLossWarningType int warningType = getAccessLossWarningType(prefService);
        if (warningType != PasswordAccessLossWarningType.NONE) {
            // Always start export flow from Chrome main settings. If this is already being called
            // from main settings, then launch export flow right away.
            Runnable startExportFlow =
                    referrer == ManagePasswordsReferrer.CHROME_SETTINGS
                            ? () -> launchExportFlow(context, profile, modalDialogManagerSupplier)
                            : () -> PasswordExportLauncher.showMainSettingsAndStartExport(context);
            new PasswordAccessLossDialogSettingsCoordinator()
                    .showPasswordAccessLossDialog(
                            context,
                            modalDialogManagerSupplier.get(),
                            warningType,
                            GmsUpdateLauncher::launch,
                            startExportFlow,
                            customTabIntentHelper);
            return true;
        }
        if (shouldShowAccessLossWarningWhenNoGmsNoPasswords(prefService, buildInfo)) {
            new PasswordAccessLossPostExportDialogController(
                            context, modalDialogManagerSupplier.get(), customTabIntentHelper)
                    .showPostExportDialog();
            return true;
        }
        return false;
    }

    /**
     * Starts the export flow (saves all user passwords on disk and deletes them from Chrome).
     *
     * @param context used to fetch the activity.
     * @param profile Chrome profile.
     * @param modalDialogManagerSupplier used for displaying modal dialogs.
     */
    public static void launchExportFlow(
            Context context,
            Profile profile,
            Supplier<ModalDialogManager> modalDialogManagerSupplier) {
        FragmentActivity activity = (FragmentActivity) ContextUtils.activityFromContext(context);
        assert activity != null : "Context is expected to be a fragment activity";

        new PasswordAccessLossExportFlowCoordinator(activity, profile, modalDialogManagerSupplier)
                .startExportFlow();
    }

    /**
     * Check which type of the warning to show (`NONE` is no dialog should be displayed).
     *
     * @param prefService used to check user prefs to decide on the type of the warning to show.
     * @return the type of the warning to display.
     */
    public static @PasswordAccessLossWarningType int getAccessLossWarningType(
            PrefService prefService) {
        // TODO(crbug.com/323149739): Enable this feature flag in SafetyCheckMediatorTest and
        // PasswordManagerHelperTest in all tests before launch.
        if (!ChromeFeatureList.isEnabled(
                ChromeFeatureList
                        .UNIFIED_PASSWORD_MANAGER_LOCAL_PASSWORDS_ANDROID_ACCESS_LOSS_WARNING)) {
            return PasswordAccessLossWarningType.NONE;
        }
        return PasswordManagerUtilBridge.getPasswordAccessLossWarningType(prefService);
    }

    /**
     * Check if the warning dialog in settings should be shown even when there are no local
     * passwords that need to be exported.
     *
     * @param prefService used to check if the login database for profile is empty.
     * @param buildInfo used to check the GMS Core version.
     * @return whether the warning dialog in settings should be shown.
     */
    public static boolean shouldShowAccessLossWarningWhenNoGmsNoPasswords(
            PrefService prefService, BuildInfo buildInfo) {
        if (!ChromeFeatureList.isEnabled(
                ChromeFeatureList
                        .UNIFIED_PASSWORD_MANAGER_LOCAL_PASSWORDS_ANDROID_ACCESS_LOSS_WARNING)) {
            return false;
        }
        try {
            Integer.parseInt(buildInfo.getGmsVersionCode());
            return false;
        } catch (NumberFormatException exception) {
            return prefService.getBoolean(Pref.EMPTY_PROFILE_STORE_LOGIN_DATABASE);
        }
    }
}
