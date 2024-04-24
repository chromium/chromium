// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.content.Context;

import androidx.annotation.NonNull;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.password_manager.PasswordCheckReferrer;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelper;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.sync.SyncService;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** An implementation of {@link SafetyHubModuleDelegate} */
public class SafetyHubModuleDelegateImpl implements SafetyHubModuleDelegate {
    private final @NonNull Profile mProfile;
    private final @NonNull Supplier<ModalDialogManager> mModalDialogManagerSupplier;

    /**
     * @param profile A supplier for {@link Profile} that owns the data being deleted.
     * @param modalDialogManagerSupplier A supplier for {@link ModalDialogManager} that will be used
     *     to launch the password check UI.
     */
    public SafetyHubModuleDelegateImpl(
            @NonNull Profile profile,
            @NonNull Supplier<ModalDialogManager> modalDialogManagerSupplier) {
        mProfile = profile;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
    }

    @Override
    public boolean shouldShowPasswordCheckModule() {
        SyncService syncService = SyncServiceFactory.getForProfile(mProfile);
        PasswordManagerHelper passwordManagerHelper = PasswordManagerHelper.getForProfile(mProfile);
        return PasswordManagerHelper.hasChosenToSyncPasswords(syncService)
                && passwordManagerHelper.canUseUpm();
    }

    @Override
    public void showPasswordCheckUI(Context context) {
        SyncService syncService = SyncServiceFactory.getForProfile(mProfile);
        PasswordManagerHelper passwordManagerHelper = PasswordManagerHelper.getForProfile(mProfile);

        assert PasswordManagerHelper.hasChosenToSyncPasswords(syncService)
                : "The password module should be hidden if the user is not syncing.";
        String account = CoreAccountInfo.getEmailFrom(syncService.getAccountInfo());

        passwordManagerHelper.showPasswordCheckup(
                context, PasswordCheckReferrer.SAFETY_CHECK, mModalDialogManagerSupplier, account);
    }
}
