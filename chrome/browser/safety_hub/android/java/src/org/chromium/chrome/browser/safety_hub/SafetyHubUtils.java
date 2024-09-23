// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.content.Context;

import androidx.annotation.Nullable;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.password_manager.PasswordCheckReferrer;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelper;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridge;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Utility methods for common Safety Hub related actions. */
class SafetyHubUtils {

    /**
     * Launches the Password Checkup UI from GMSCore.
     *
     * @param context used to show the dialog.
     */
    static void showPasswordCheckUI(
            Context context,
            Profile profile,
            Supplier<ModalDialogManager> modalDialogManagerSupplier) {
        PasswordManagerHelper passwordManagerHelper = PasswordManagerHelper.getForProfile(profile);
        String account = getAccountEmail(profile);
        assert account != null
                : "The password check UI should only be launched for signed in Safety Hub users.";
        passwordManagerHelper.showPasswordCheckup(
                context, PasswordCheckReferrer.SAFETY_CHECK, modalDialogManagerSupplier, account);
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
    @Nullable
    static String getAccountEmail(Profile profile) {
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
}
