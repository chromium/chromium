// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.content.Context;

import org.jni_zero.CalledByNative;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Bridge between Java and native PasswordManager code. */
public class PasswordManagerLauncher {
    private PasswordManagerLauncher() {}

    /**
     * Launches the password settings.
     *
     * @param context current activity context
     * @param referer specifies on whose behalf the PasswordManager will be opened
     * @param modalDialogManagerSupplier ModalDialogManager supplier to be used by loading dialog.
     * @param managePasskeys the content to be managed
     */
    public static void showPasswordSettings(
            Context context,
            @ManagePasswordsReferrer int referrer,
            Supplier<ModalDialogManager> modalDialogManagerSupplier,
            boolean managePasskeys) {
        PasswordManagerHelper.showPasswordSettings(
                context,
                referrer,
                new SettingsLauncherImpl(),
                SyncServiceFactory.get(),
                modalDialogManagerSupplier,
                managePasskeys);
    }

    @CalledByNative
    private static void showPasswordSettings(
            WebContents webContents,
            @ManagePasswordsReferrer int referrer,
            boolean managePasskeys) {
        WindowAndroid window = webContents.getTopLevelNativeWindow();
        if (window == null) return;
        showPasswordSettings(
                window.getActivity().get(),
                referrer,
                () -> window.getModalDialogManager(),
                managePasskeys);
    }

    @CalledByNative
    private static boolean canManagePasswordsWhenPasskeysPresent() {
        return PasswordManagerHelper.canUseUpm() || !PasswordManagerHelper.canUseAccountSettings();
    }
}
