// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.password_manager;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.password_check.PasswordCheckFactory;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * A utitily class for launching the password leak check.
 */
public class PasswordCheckupLauncher {
    @CalledByNative
    private static void launchCheckupInAccountWithWindowAndroid(
            String checkupUrl, WindowAndroid windowAndroid) {
        if (windowAndroid.getContext().get() == null) return; // Window not available yet/anymore.
        launchCheckupInAccountWithActivity(checkupUrl, windowAndroid.getActivity().get());
    }

    @CalledByNative
    private static void launchLocalCheckup(
            WindowAndroid windowAndroid, @PasswordCheckReferrer int passwordCheckReferrer) {
        if (windowAndroid.getContext().get() == null) return; // Window not available yet/anymore.

        if (PasswordManagerHelper.canUseUpm()) {
            PasswordManagerHelper.showPasswordCheckup(windowAndroid.getContext().get(),
                    passwordCheckReferrer, SyncServiceFactory.get(),
                    getModalDialogManagerSupplier(windowAndroid));
            return;
        }

        PasswordCheckFactory.getOrCreate(new SettingsLauncherImpl())
                .showUi(windowAndroid.getContext().get(), passwordCheckReferrer);
    }

    @CalledByNative
    private static void launchCheckupInAccountWithActivity(String checkupUrl, Activity activity) {
        if (tryLaunchingNativePasswordCheckup(activity)) return;
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(checkupUrl));
        intent.setPackage(activity.getPackageName());
        activity.startActivity(intent);
    }

    private static boolean tryLaunchingNativePasswordCheckup(Activity activity) {
        GooglePasswordManagerUIProvider googlePasswordManagerUIProvider =
                AppHooks.get().createGooglePasswordManagerUIProvider();
        if (googlePasswordManagerUIProvider == null) return false;
        return googlePasswordManagerUIProvider.launchPasswordCheckup(activity);
    }

    private static ObservableSupplier<ModalDialogManager> getModalDialogManagerSupplier(
            WindowAndroid windowAndroid) {
        ObservableSupplierImpl<ModalDialogManager> modalDialogManagerSupplier =
                new ObservableSupplierImpl<>();
        modalDialogManagerSupplier.set(windowAndroid.getModalDialogManager());
        return modalDialogManagerSupplier;
    }
}
