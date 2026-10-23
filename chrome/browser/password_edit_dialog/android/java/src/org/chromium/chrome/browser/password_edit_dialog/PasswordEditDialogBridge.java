// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_edit_dialog;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniPtr;
import org.jni_zero.JniRawPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid;

/**
 * Java part of PasswordEditBridge pair providing communication between native password manager code
 * and Java password edit dialog UI components.
 */
@NullMarked
public class PasswordEditDialogBridge implements PasswordEditDialogCoordinator.Delegate {
    @JniType("::PasswordEditDialogBridge")
    interface NativePasswordEditDialogBridge extends JniTypeToken {}

    private @Nullable JniRawPtr<NativePasswordEditDialogBridge> mNativeDialog;
    private final PasswordEditDialogCoordinator mDialogCoordinator;

    @CalledByNative
    static PasswordEditDialogBridge create(
            JniRawPtr<NativePasswordEditDialogBridge> nativeDialog, WindowAndroid windowAndroid) {
        return new PasswordEditDialogBridge(nativeDialog, windowAndroid);
    }

    private PasswordEditDialogBridge(
            JniRawPtr<NativePasswordEditDialogBridge> nativeDialog, WindowAndroid windowAndroid) {
        mNativeDialog = nativeDialog;
        mDialogCoordinator = PasswordEditDialogCoordinator.create(windowAndroid, this);
    }

    @CalledByNative
    void showPasswordEditDialog(
            @JniType("std::vector<std::u16string>") String[] savedUsernames,
            @JniType("std::u16string") String username,
            @JniType("std::u16string") String password,
            @JniType("std::optional<std::string>") @Nullable String account,
            boolean isSavingBlockedByTrustedVaultError) {
        mDialogCoordinator.showPasswordEditDialog(
                savedUsernames, username, password, account, isSavingBlockedByTrustedVaultError);
    }

    @CalledByNative
    void dismiss() {
        mDialogCoordinator.dismiss();
    }

    @Override
    public void onDialogAccepted(String username, String password) {
        assert mNativeDialog != null;
        PasswordEditDialogBridgeJni.get().onDialogAccepted(mNativeDialog, username, password);
    }

    @Override
    public void onDialogDismissed(boolean dialogAccepted) {
        assert mNativeDialog != null;
        PasswordEditDialogBridgeJni.get().onDialogDismissed(mNativeDialog, dialogAccepted);
        mNativeDialog.release();
        mNativeDialog = null;
    }

    @Override
    public boolean isUsingAccountStorage(String username) {
        assert mNativeDialog != null;
        return PasswordEditDialogBridgeJni.get().isUsingAccountStorage(mNativeDialog, username);
    }

    @NativeMethods
    interface Natives {
        void onDialogAccepted(
                JniPtr<NativePasswordEditDialogBridge> self,
                @JniType("std::u16string") String username,
                @JniType("std::u16string") String password);

        void onDialogDismissed(JniPtr<NativePasswordEditDialogBridge> self, boolean dialogAccepted);

        boolean isUsingAccountStorage(
                JniPtr<NativePasswordEditDialogBridge> self,
                @JniType("std::u16string") String username);
    }
}
