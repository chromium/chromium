// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.Context;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsCustomTabLauncherImpl;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.sync.SyncService;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.function.Supplier;

/** Bridge between Java and native PasswordManager code. */
@NullMarked
public class PasswordManagerLauncher {
    private PasswordManagerLauncher() {}

    /**
     * Launches the password settings.
     *
     * @param context current activity context
     * @param profile the {@link Profile} associated with the passwords.
     * @param referrer specifies on whose behalf the PasswordManager will be opened
     * @param modalDialogManagerSupplier ModalDialogManager supplier to be used by loading dialog.
     * @param managePasskeys the content to be managed
     */
    public static void showPasswordSettings(
            Context context,
            Profile profile,
            @ManagePasswordsReferrer int referrer,
            Supplier<@Nullable ModalDialogManager> modalDialogManagerSupplier,
            boolean managePasskeys) {
        assert profile != null;
        Profile originalProfile = profile.getOriginalProfile();
        SyncService syncService = SyncServiceFactory.getForProfile(profile);
        String account =
                PasswordManagerHelper.hasChosenToSyncPasswords(syncService)
                        ? CoreAccountInfo.getEmailFrom(assumeNonNull(syncService).getAccountInfo())
                        : null;
        PasswordManagerHelper.getForProfile(originalProfile)
                .showPasswordSettings(
                        context,
                        referrer,
                        modalDialogManagerSupplier,
                        managePasskeys,
                        account,
                        new SettingsCustomTabLauncherImpl());
    }

    @CalledByNative
    private static void showPasswordSettings(
            WebContents webContents,
            @ManagePasswordsReferrer int referrer,
            boolean managePasskeys) {
        WindowAndroid window = webContents.getTopLevelNativeWindow();
        if (window == null) return;
        Activity context = window.getActivity().get();
        assert context != null;
        showPasswordSettings(
                context,
                Profile.fromWebContents(webContents),
                referrer,
                () -> window.getModalDialogManager(),
                managePasskeys);
    }
}
