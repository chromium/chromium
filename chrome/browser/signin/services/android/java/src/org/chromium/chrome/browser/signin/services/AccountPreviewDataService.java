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
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.base.AccountInfo;

import java.util.List;

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

    /**
     * Returns the preferred account if present among accounts, or the first account as default. The
     * accounts list must not be empty.
     *
     * <p>TODO(crbug.com/553530451): Remove this method once FullscreenSigninMediator is migrated.
     */
    @MainThread
    public AccountInfo getPreferredAccountOrDefault(List<AccountInfo> accounts) {
        ThreadUtils.assertOnUiThread();
        assert !accounts.isEmpty();
        assert SigninFeatureMap.isEnabled(SigninFeatures.ENABLE_ACCOUNT_PREVIEW_PREFERRED_ACCOUNT);

        AccountPreviewPreference preference = getPreferredAccountForPromo();
        AccountInfo account =
                preference != null
                        ? AccountUtils.findAccountByGaiaId(accounts, preference.getGaiaId())
                        : null;
        return account != null ? account : accounts.get(0);
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
