// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.add_username_dialog;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid;

@NullMarked
public class AddUsernameDialogBridge implements AddUsernameDialogController.Delegate {
    private long mNativeAddUsernameDialogBridge;
    private final WindowAndroid mWindowAndroid;
    private @Nullable AddUsernameDialogController mController;

    @CalledByNative
    public AddUsernameDialogBridge(
            long nativeAddUsernameDialogBridge, WindowAndroid windowAndroid) {
        mNativeAddUsernameDialogBridge = nativeAddUsernameDialogBridge;
        mWindowAndroid = windowAndroid;
    }

    @CalledByNative
    public void showAddUsernameDialog(@JniType("std::u16string") String password) {
        Context context = mWindowAndroid.getContext().get();
        if (context == null) return;

        mController =
                new AddUsernameDialogController(
                        context, assumeNonNull(mWindowAndroid.getModalDialogManager()), this);
        mController.showAddUsernameDialog(password);
    }

    @CalledByNative
    public void dismiss() {
        assert mController != null : "Must not call `dismiss` before `showAddUsernameDialog`";
        mController.dismissDialog();
        mNativeAddUsernameDialogBridge = 0;
    }

    @Override
    public void onDialogAccepted(String username) {
        if (mNativeAddUsernameDialogBridge == 0) return;
        AddUsernameDialogBridgeJni.get().onDialogAccepted(mNativeAddUsernameDialogBridge, username);
    }

    @Override
    public void onDialogDismissed() {
        if (mNativeAddUsernameDialogBridge == 0) return;
        AddUsernameDialogBridgeJni.get().onDialogDismissed(mNativeAddUsernameDialogBridge);
        mNativeAddUsernameDialogBridge = 0;
    }

    @NativeMethods
    interface Natives {
        void onDialogAccepted(
                long nativeAddUsernameDialogBridge, @JniType("std::u16string") String username);

        void onDialogDismissed(long nativeAddUsernameDialogBridge);
    }
}
