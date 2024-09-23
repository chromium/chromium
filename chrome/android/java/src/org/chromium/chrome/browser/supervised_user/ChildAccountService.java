// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.supervised_user;

import android.accounts.Account;
import android.app.Activity;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.ui.base.WindowAndroid;

/** This class serves as a simple interface for native code to re-authenticate a child account. */
public class ChildAccountService {
    private ChildAccountService() {
        // Only for static usage.
    }

    @VisibleForTesting
    @CalledByNative
    static void reauthenticateChildAccount(
            WindowAndroid windowAndroid,
            @JniType("std::string") String accountName,
            final long nativeOnFailureCallback) {
        ThreadUtils.assertOnUiThread();
        final Activity activity = windowAndroid.getActivity().get();
        if (activity == null) {
            PostTask.postTask(
                    TaskTraits.UI_DEFAULT,
                    () -> {
                        ChildAccountServiceJni.get()
                                .onReauthenticationFailed(nativeOnFailureCallback);
                    });
            return;
        }
        Account account = AccountUtils.createAccountFromName(accountName);
        AccountManagerFacadeProvider.getInstance()
                .updateCredentials(
                        account,
                        activity,
                        success -> {
                            if (!success) {
                                ChildAccountServiceJni.get()
                                        .onReauthenticationFailed(nativeOnFailureCallback);
                            }
                        });
    }

    @NativeMethods
    interface Natives {
        void onReauthenticationFailed(long onFailureCallbackPtr);
    }
}
