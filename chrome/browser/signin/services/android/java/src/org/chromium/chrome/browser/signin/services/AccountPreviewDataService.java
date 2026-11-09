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

import org.chromium.base.Callback;
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

    /**
     * Returns the preferred account and preferred data types when offering signed-out users to sign
     * in.
     */
    @MainThread
    public @Nullable AccountPreviewPreference getPreferredAccountForSignin() {
        ThreadUtils.assertOnUiThread();
        return AccountPreviewDataServiceJni.get()
                .getPreferredAccountForSignin(mNativeAccountPreviewDataService);
    }

    /**
     * Asynchronous version of {@link #getPreferredAccountForSignin()}, which returns the stored
     * preferred account if available, or triggers a fetch and waits for the preferred account
     * computation to complete.
     *
     * <p>{@code callback} is always run asynchronously once, with the preferred account or {@code
     * null} if unavailable or if the computation does not complete within {@code timeoutMs}
     * milliseconds.
     */
    @MainThread
    public void getPreferredAccountForSigninAsync(
            Callback<@Nullable AccountPreviewPreference> callback, long timeoutMs) {
        ThreadUtils.assertOnUiThread();
        AccountPreviewDataServiceJni.get()
                .getPreferredAccountForSigninAsync(
                        mNativeAccountPreviewDataService, callback, timeoutMs);
    }

    /**
     * Returns the maximum duration in milliseconds the FRE waits for an ongoing preferred account
     * computation to update the selected account.
     */
    @MainThread
    public long getPreferredAccountFREFetchTimeoutMs() {
        ThreadUtils.assertOnUiThread();
        return AccountPreviewDataServiceJni.get().getPreferredAccountFREFetchTimeoutMs();
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
        @Nullable AccountPreviewPreference getPreferredAccountForSignin(
                @JniType("AccountPreviewDataService*") long nativeAccountPreviewDataService);

        void getPreferredAccountForSigninAsync(
                long accountPreviewDataServicePtr,
                @JniType("signin::AccountPreviewPreferenceCallback")
                        Callback<@Nullable AccountPreviewPreference> callback,
                long timeoutMs);

        long getPreferredAccountFREFetchTimeoutMs();

        void updateExternalAppAccount(
                @JniType("AccountPreviewDataService*") long nativeAccountPreviewDataService,
                @JniType("std::optional<std::string>") @Nullable String email);
    }
}
