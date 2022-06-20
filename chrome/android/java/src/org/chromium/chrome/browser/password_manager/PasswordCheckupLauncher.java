// Copyright 2019 The Chromium Authors. All rights reserved.
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
import org.chromium.chrome.browser.sync.SyncService;
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
    // TODO(crbug.com/1311952): Merge with launchLocalCheckupFromPhishGuardWarningDialog.
    private static void launchLocalCheckup(WindowAndroid windowAndroid) {
        if (windowAndroid.getContext().get() == null) return; // Window not available yet/anymore.

        if (PasswordManagerHelper.canUseUpmCheckup()) {
            PasswordCheckupClientHelper checkupHelper =
                    PasswordCheckupClientHelperFactory.getInstance().createHelper();
            if (checkupHelper != null) {
                PasswordManagerHelper.showPasswordCheckup(windowAndroid.getContext().get(),
                        PasswordCheckReferrer.LEAK_DIALOG,
                        PasswordCheckupClientHelperFactory.getInstance().createHelper(),
                        SyncService.get(), getModalDialogManagerSupplier(windowAndroid));
                return;
            }
        }

        PasswordCheckFactory.getOrCreate(new SettingsLauncherImpl())
                .showUi(windowAndroid.getContext().get(), PasswordCheckReferrer.LEAK_DIALOG);
    }

    @CalledByNative
    // TODO(crbug.com/1311952): Merge with launchLocalCheckup.
    private static void launchLocalCheckupFromPhishGuardWarningDialog(WindowAndroid windowAndroid) {
        if (windowAndroid.getContext().get() == null) return; // Window not available yet/anymore.

        if (PasswordManagerHelper.canUseUpmCheckup()) {
            PasswordCheckupClientHelper checkupHelper =
                    PasswordCheckupClientHelperFactory.getInstance().createHelper();
            if (checkupHelper != null) {
                PasswordManagerHelper.showPasswordCheckup(windowAndroid.getContext().get(),
                        PasswordCheckReferrer.PHISHED_WARNING_DIALOG,
                        PasswordCheckupClientHelperFactory.getInstance().createHelper(),
                        SyncService.get(), getModalDialogManagerSupplier(windowAndroid));
                return;
            }
        }
        PasswordCheckFactory.getOrCreate(new SettingsLauncherImpl())
                .showUi(windowAndroid.getContext().get(),
                        PasswordCheckReferrer.PHISHED_WARNING_DIALOG);
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
