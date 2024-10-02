// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.password_manager;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.password_check.PasswordCheckFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.browser_ui.settings.SettingsNavigation.SettingsFragment;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** A utitily class for launching the password leak check. */
public class PasswordCheckupLauncher {
    @CalledByNative
    private static void launchCheckupOnlineWithWindowAndroid(
            String checkupUrl, WindowAndroid windowAndroid) {
        if (windowAndroid.getContext().get() == null) return; // Window not available yet/anymore.
        launchCheckupOnlineWithActivity(checkupUrl, windowAndroid.getActivity().get());
    }

    @CalledByNative
    static void launchCheckupOnDevice(
            Profile profile,
            WindowAndroid windowAndroid,
            @PasswordCheckReferrer int passwordCheckReferrer,
            @Nullable String accountEmail) {
        assert accountEmail == null || !accountEmail.isEmpty();
        if (windowAndroid.getContext().get() == null) return; // Window not available yet/anymore.

        assert profile != null;
        PasswordManagerHelper passwordManagerHelper = PasswordManagerHelper.getForProfile(profile);
        // Force instantiation of GMSCore password check if GMSCore update is required. Password
        // check launch will fail and instead show the blocking dialog with the suggestion to
        // update.
        if (passwordManagerHelper.canUseUpm()
                || PasswordManagerUtilBridge.isGmsCoreUpdateRequired(
                        UserPrefs.get(profile), SyncServiceFactory.getForProfile(profile))) {
            passwordManagerHelper.showPasswordCheckup(
                    windowAndroid.getContext().get(),
                    passwordCheckReferrer,
                    getModalDialogManagerSupplier(windowAndroid),
                    accountEmail);
            return;
        }

        PasswordCheckFactory.getOrCreate()
                .showUi(windowAndroid.getContext().get(), passwordCheckReferrer);
    }

    @CalledByNative
    private static void launchCheckupOnlineWithActivity(String checkupUrl, Activity activity) {
        if (tryLaunchingNativePasswordCheckup(activity)) return;
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(checkupUrl));
        intent.setPackage(activity.getPackageName());
        activity.startActivity(intent);
    }

    @CalledByNative
    static void launchSafetyCheck(WindowAndroid windowAndroid) {
        if (windowAndroid.getContext().get() == null) return; // Window not available yet/anymore.
        SettingsNavigationFactory.createSettingsNavigation()
                .startSettings(windowAndroid.getContext().get(), SettingsFragment.SAFETY_CHECK);
    }

    private static boolean tryLaunchingNativePasswordCheckup(Activity activity) {
        GooglePasswordManagerUIProvider googlePasswordManagerUIProvider =
                ServiceLoaderUtil.maybeCreate(GooglePasswordManagerUIProvider.class);
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
