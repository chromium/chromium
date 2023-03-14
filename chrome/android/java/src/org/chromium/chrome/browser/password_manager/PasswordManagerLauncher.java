// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.app.Activity;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.lang.ref.WeakReference;

/**
 * Bridge between Java and native PasswordManager code.
 */
public class PasswordManagerLauncher {
    private PasswordManagerLauncher() {}

    /**
     * Launches the password settings.
     *
     * @param activity used to show the UI to manage passwords.
     */
    public static void showPasswordSettings(Activity activity,
            @ManagePasswordsReferrer int referrer,
            ObservableSupplier<ModalDialogManager> modalDialogManagerSupplier,
            boolean managePasskeys) {
        SyncService syncService = SyncService.get();
        PasswordManagerHelper.showPasswordSettings(activity, referrer, new SettingsLauncherImpl(),
                syncService, modalDialogManagerSupplier, managePasskeys);
    }

    @CalledByNative
    private static void showPasswordSettings(WebContents webContents,
            @ManagePasswordsReferrer int referrer, boolean managePasskeys) {
        WindowAndroid window = webContents.getTopLevelNativeWindow();
        if (window == null) return;
        WeakReference<Activity> currentActivity = window.getActivity();
        ObservableSupplierImpl<ModalDialogManager> modalDialogManagerSupplier =
                new ObservableSupplierImpl<>();
        modalDialogManagerSupplier.set(window.getModalDialogManager());
        showPasswordSettings(
                currentActivity.get(), referrer, modalDialogManagerSupplier, managePasskeys);
    }

    @CalledByNative
    private static boolean canManagePasswordsWhenPasskeysPresent() {
        return PasswordManagerHelper.canUseUpm() || !PasswordManagerHelper.canUseAccountSettings();
    }
}
