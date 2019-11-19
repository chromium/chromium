// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.childaccounts;

import android.accounts.Account;
import android.app.Activity;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.task.PostTask;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.ChildAccountStatus;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.ui.base.WindowAndroid;

/**
 * This class serves as a simple interface for querying the child account information. It has a
 * method for querying the child account information asynchronously from the system.
 *
 * This method is used by ForcedSigninProcessor and FirstRunFlowSequencer to detect child accounts
 * since the native side is only activated on signing in. Once signed in by the
 * ForcedSigninProcessor, the ChildAccountInfoFetcher will notify the native side and also takes
 * responsibility for monitoring changes and taking a suitable action.
 *
 * The class also provides an interface through which a client can listen for child account status
 * changes. When the SupervisedUserContentProvider forces sign-in it waits for a status change
 * before querying the URL filters.
 */
public class ChildAccountService {
    private ChildAccountService() {
        // Only for static usage.
    }

    /**
     * Checks for the presence of child accounts on the device.
     *
     * @param callback A callback which will be called with a @ChildAccountStatus.Status value.
     */
    public static void checkChildAccountStatus(final Callback<Integer> callback) {
        ThreadUtils.assertOnUiThread();
        final AccountManagerFacade accountManager = AccountManagerFacade.get();
        accountManager.tryGetGoogleAccounts(accounts -> {
            if (accounts.size() != 1) {
                // Child accounts can't share a device.
                callback.onResult(ChildAccountStatus.NOT_CHILD);
            } else {
                accountManager.checkChildAccountStatus(accounts.get(0), callback);
            }
        });
    }

    /**
     * Set a callback to be called the next time a child account status change is received
     * @param callback the callback to be called when the status changes.
     */
    public static void listenForStatusChange(Callback<Boolean> callback) {
        ChildAccountServiceJni.get().listenForChildStatusReceived(callback);
    }

    @CalledByNative
    private static void reauthenticateChildAccount(
            WindowAndroid windowAndroid, String accountName, final long nativeCallback) {
        ThreadUtils.assertOnUiThread();

        Activity activity = windowAndroid.getActivity().get();
        if (activity == null) {
            PostTask.postTask(UiThreadTaskTraits.DEFAULT,
                    ()
                            -> ChildAccountServiceJni.get().onReauthenticationResult(
                                    nativeCallback, false));
            return;
        }

        Account account = AccountManagerFacade.createAccountFromName(accountName);
        AccountManagerFacade.get().updateCredentials(account, activity,
                result
                -> ChildAccountServiceJni.get().onReauthenticationResult(nativeCallback, result));
    }

    @NativeMethods
    interface Natives {
        void listenForChildStatusReceived(Callback<Boolean> callback);
        void onReauthenticationResult(long callbackPtr, boolean reauthSuccessful);
    }
}
