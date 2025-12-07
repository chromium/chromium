// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.password_manager;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.safety_check.SafetyCheckSettingsFragment;
import org.chromium.chrome.browser.settings.SettingsCustomTabLauncherImpl;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.components.browser_ui.settings.SettingsNavigation.SettingsFragment;
import org.chromium.ui.base.WindowAndroid;

/** A utility class for launching the password leak check. */
@NullMarked
public class PasswordCheckupLauncher {
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
        // This is invoked from the leak dialog if the compromised password is saved for other
        // sites. After the login DB deprecation, this code path is guaranteed to only be
        // executed for users with access to UPM, since they are the only ones with saved
        // passwords.
        passwordManagerHelper.showPasswordCheckup(
                windowAndroid.getContext().get(),
                passwordCheckReferrer,
                () -> windowAndroid.getModalDialogManager(),
                accountEmail,
                new SettingsCustomTabLauncherImpl());
    }

    @CalledByNative
    static void launchSafetyCheck(WindowAndroid windowAndroid) {
        if (windowAndroid.getContext().get() == null) return; // Window not available yet/anymore.
        SettingsNavigationFactory.createSettingsNavigation()
                .startSettings(
                        windowAndroid.getContext().get(),
                        SafetyCheckSettingsFragment.class,
                        SafetyCheckSettingsFragment.createBundle(true));
    }

    @CalledByNative
    static void launchSafetyHub(WindowAndroid windowAndroid) {
        if (windowAndroid.getContext().get() == null) return; // Window not available yet/anymore.
        SettingsNavigationFactory.createSettingsNavigation()
                .startSettings(windowAndroid.getContext().get(), SettingsFragment.SAFETY_CHECK);
    }
}
