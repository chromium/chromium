// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.childaccounts;

import android.accounts.Account;
import android.app.Activity;

import androidx.annotation.MainThread;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.task.PostTask;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacade.ChildAccountStatusListener;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
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
     * Checks the child account status on device.
     *
     * Since child accounts cannot share a device, the listener will be invoked with the status
     * {@link ChildAccountStatus#NOT_CHILD} if there are no accounts or more than one account on
     * device. If there is a single account on device, the listener will be passed to
     * {@link AccountManagerFacade#checkChildAccountStatus} to check the child account status of
     * the account.
     *
     * It should be safe to invoke this method before the native library is initialized (after
     * AccountManagerFacade is set).
     *
     * @param listener The listener is called when the {@link ChildAccountStatus.Status} is ready.
     */
    @MainThread
    public static void checkChildAccountStatus(ChildAccountStatusListener listener) {
        ThreadUtils.assertOnUiThread();
        final AccountManagerFacade accountManagerFacade =
                AccountManagerFacadeProvider.getInstance();
        accountManagerFacade.tryGetGoogleAccounts(accounts -> {
            if (accounts.size() == 1) {
                // Child accounts can't share a device.
                accountManagerFacade.checkChildAccountStatus(accounts.get(0), listener);
            } else {
                listener.onStatusReady(ChildAccountStatus.NOT_CHILD);
            }
        });
    }

    @VisibleForTesting
    @CalledByNative
    static void reauthenticateChildAccount(
            WindowAndroid windowAndroid, String accountName, final long nativeOnFailureCallback) {
        ThreadUtils.assertOnUiThread();
        final Activity activity = windowAndroid.getActivity().get();
        if (activity == null) {
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> {
                ChildAccountServiceJni.get().onReauthenticationFailed(nativeOnFailureCallback);
            });
            return;
        }
        Account account = AccountUtils.createAccountFromName(accountName);
        AccountManagerFacadeProvider.getInstance().updateCredentials(account, activity, success -> {
            if (!success) {
                ChildAccountServiceJni.get().onReauthenticationFailed(nativeOnFailureCallback);
            }
        });
    }

    @NativeMethods
    interface Natives {
        void onReauthenticationFailed(long onFailureCallbackPtr);
    }
}
