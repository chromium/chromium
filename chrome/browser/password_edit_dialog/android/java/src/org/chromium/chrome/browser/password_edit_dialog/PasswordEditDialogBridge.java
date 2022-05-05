// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_edit_dialog;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.ui.base.WindowAndroid;

/**
 * Java part of PasswordEditBridge pair providing communication between native password manager code
 * and Java password edit dialog UI components.
 */
public class PasswordEditDialogBridge {
    private long mNativeDialog;

    @CalledByNative
    static PasswordEditDialogBridge create(
            long nativeDialog, @NonNull WindowAndroid windowAndroid) {
        return new PasswordEditDialogBridge(nativeDialog, windowAndroid);
    }

    private PasswordEditDialogBridge(long nativeDialog, @NonNull WindowAndroid windowAndroid) {
        mNativeDialog = nativeDialog;
    }

    @CalledByNative
    void show(@NonNull String[] usernames, int selectedUsernameIndex, @NonNull String password,
            @NonNull String origin, @Nullable String account) {
    }

    @CalledByNative
    void dismiss() {

    }

    @NativeMethods
    interface Natives {
        void onDialogAccepted(long nativePasswordEditDialogBridge, int selectedUsernameIndex);
        void onDialogDismissed(long nativePasswordEditDialogBridge, boolean dialogAccepted);
    }
}