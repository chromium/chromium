// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.add_username_dialog;

import android.content.Context;

import androidx.annotation.NonNull;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.ui.base.WindowAndroid;

public class AddUsernameDialogBridge implements AddUsernameDialogController.Delegate {
    private long mNativeAddUsernameDialogBridge;
    private final WindowAndroid mWindowAndroid;

    @CalledByNative
    public AddUsernameDialogBridge(
            long nativeAddUsernameDialogBridge, @NonNull WindowAndroid windowAndroid) {
        mNativeAddUsernameDialogBridge = nativeAddUsernameDialogBridge;
        mWindowAndroid = windowAndroid;
    }

    @CalledByNative
    public void showAddUsernameDialog(String password) {
        Context context = mWindowAndroid.getContext().get();
        if (context == null) return;

        AddUsernameDialogController controller = new AddUsernameDialogController(
                context, mWindowAndroid.getModalDialogManager(), this);
        controller.showAddUsernameDialog(password);
    }

    @Override
    public void onDialogAccepted(String username) {
        assert mNativeAddUsernameDialogBridge
                != 0 : "mNativeAddUsernameDialogBridge must not be null";
        AddUsernameDialogBridgeJni.get().onDialogAccepted(mNativeAddUsernameDialogBridge, username);
    }

    @Override
    public void onDialogDismissed() {
        assert mNativeAddUsernameDialogBridge
                != 0 : "mNativeAddUsernameDialogBridge must not be null";
        AddUsernameDialogBridgeJni.get().onDialogDismissed(mNativeAddUsernameDialogBridge);
        mNativeAddUsernameDialogBridge = 0;
        // TODO(https://crbug.com/1421753): introduce the AddUsernameDialogBridge.dismiss() method
        // to dismiss the dialog from C++.
    }

    @NativeMethods
    interface Natives {
        void onDialogAccepted(long nativeAddUsernameDialogBridge, String username);
        void onDialogDismissed(long nativeAddUsernameDialogBridge);
    }
}
