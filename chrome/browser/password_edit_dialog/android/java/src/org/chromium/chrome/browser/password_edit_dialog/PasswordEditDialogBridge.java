// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_edit_dialog;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
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
    private long mNativeDialog;
    private final PasswordEditDialogCoordinator mDialogCoordinator;

    @CalledByNative
    static PasswordEditDialogBridge create(long nativeDialog, WindowAndroid windowAndroid) {
        return new PasswordEditDialogBridge(nativeDialog, windowAndroid);
    }

    private PasswordEditDialogBridge(long nativeDialog, WindowAndroid windowAndroid) {
        mNativeDialog = nativeDialog;
        mDialogCoordinator = PasswordEditDialogCoordinator.create(windowAndroid, this);
    }

    @CalledByNative
    void showPasswordEditDialog(
            String[] savedUsernames,
            @JniType("std::u16string") String username,
            @JniType("std::u16string") String password,
            @Nullable String account) {
        mDialogCoordinator.showPasswordEditDialog(savedUsernames, username, password, account);
    }

    @CalledByNative
    void dismiss() {
        mDialogCoordinator.dismiss();
    }

    @Override
    public void onDialogAccepted(String username, String password) {
        assert mNativeDialog != 0;
        PasswordEditDialogBridgeJni.get().onDialogAccepted(mNativeDialog, username, password);
    }

    @Override
    public void onDialogDismissed(boolean dialogAccepted) {
        assert mNativeDialog != 0;
        PasswordEditDialogBridgeJni.get().onDialogDismissed(mNativeDialog, dialogAccepted);
        mNativeDialog = 0;
    }

    @Override
    public boolean isUsingAccountStorage(String username) {
        assert mNativeDialog != 0;
        return PasswordEditDialogBridgeJni.get().isUsingAccountStorage(mNativeDialog, username);
    }

    @NativeMethods
    interface Natives {
        void onDialogAccepted(
                long nativePasswordEditDialogBridge,
                @JniType("std::u16string") String username,
                @JniType("std::u16string") String password);

        void onDialogDismissed(long nativePasswordEditDialogBridge, boolean dialogAccepted);

        boolean isUsingAccountStorage(
                long nativePasswordEditDialogBridge, @JniType("std::u16string") String username);
    }
}
