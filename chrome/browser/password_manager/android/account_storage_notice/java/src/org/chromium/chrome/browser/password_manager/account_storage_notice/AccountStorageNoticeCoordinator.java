// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.account_storage_notice;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.ui.base.WindowAndroid;

/** Coordinator for the UI described in account_storage_notice.h, meant to be used from native. */
class AccountStorageNoticeCoordinator {
    private long mNativeCoordinatorObserver;

    @CalledByNative
    public AccountStorageNoticeCoordinator(
            WindowAndroid windowAndroid,
            SettingsLauncher settingsLauncher,
            long nativeCoordinatorObserver) {
        assert nativeCoordinatorObserver != 0;
        mNativeCoordinatorObserver = nativeCoordinatorObserver;
        // TODO(crbug.com/338576301): Show UI and only call onAccepted() once
        // interaction is done.
        AccountStorageNoticeCoordinatorJni.get().onAccepted(mNativeCoordinatorObserver);
    }

    @CalledByNative
    public void destroy() {
        // Avoid dangling pointer.
        mNativeCoordinatorObserver = 0;
    }

    @NativeMethods
    interface Natives {
        // See docs in account_storage_notice.h as to when this should be called.
        void onAccepted(long nativeCoordinatorObserver);
    }
}
