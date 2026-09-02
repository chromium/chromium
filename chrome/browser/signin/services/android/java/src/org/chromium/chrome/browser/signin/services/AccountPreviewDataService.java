// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import androidx.annotation.MainThread;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * AccountPreviewDataService provides access to native AccountPreviewDataService's public API to
 * Java components.
 */
@NullMarked
@JNINamespace("signin")
public class AccountPreviewDataService {
    private final long mNativeAccountPreviewDataService;

    @CalledByNative
    @VisibleForTesting
    AccountPreviewDataService(long nativeAccountPreviewDataService) {
        assert nativeAccountPreviewDataService != 0;
        mNativeAccountPreviewDataService = nativeAccountPreviewDataService;
    }

    /** Returns the preferred account and preferred data types for promos. */
    @MainThread
    public @Nullable AccountPreviewPreference getPreferredAccountForPromo() {
        ThreadUtils.assertOnUiThread();
        return AccountPreviewDataServiceJni.get()
                .getPreferredAccountForPromo(mNativeAccountPreviewDataService);
    }

    /** Updates the account currently used by the external app. */
    @MainThread
    public void updateExternalAppAccount(@Nullable String email) {
        ThreadUtils.assertOnUiThread();
        AccountPreviewDataServiceJni.get()
                .updateExternalAppAccount(mNativeAccountPreviewDataService, email);
    }

    @NativeMethods
    interface Natives {
        @JniType("std::optional<signin::AccountPreviewDataService::AccountPreviewPreference>")
        @Nullable AccountPreviewPreference getPreferredAccountForPromo(
                @JniType("AccountPreviewDataService*") long nativeAccountPreviewDataService);

        void updateExternalAppAccount(
                @JniType("AccountPreviewDataService*") long nativeAccountPreviewDataService,
                @JniType("std::optional<std::string>") @Nullable String email);
    }
}
