// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.supervised_user;

import androidx.annotation.NonNull;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.android_webview.common.SafeModeAction;
import org.chromium.android_webview.common.SafeModeActionIds;

/**
 * A {@link SafeModeAction} to disable restricted content blocking.
 *
 * <p>Lifetime: Singleton
 */
@JNINamespace("android_webview")
@Lifetime.Singleton
public class AwSupervisedUserSafeModeAction implements SafeModeAction {
    private static final String TAG = "WebViewSafeMode";

    // This ID should not be changed or reused.
    private static final String ID = SafeModeActionIds.DISABLE_SUPERVISION_CHECKS;

    @Override
    @NonNull
    public String getId() {
        return ID;
    }

    public static boolean isSupervisionEnabled() {
        return AwSupervisedUserSafeModeActionJni.get().isSupervisionEnabled();
    }

    public static void resetForTesting() {
        AwSupervisedUserSafeModeActionJni.get().setSupervisionEnabled(true);
    }

    @Override
    public boolean execute() {
        AwSupervisedUserSafeModeActionJni.get().setSupervisionEnabled(false);
        return true;
    }

    @NativeMethods
    interface Natives {
        void setSupervisionEnabled(boolean value);

        boolean isSupervisionEnabled();
    }
}
