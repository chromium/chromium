// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.services;

import android.accounts.AccountManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.init.BrowserParts;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.init.EmptyBrowserParts;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.SigninHelperProvider;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.signin.AccountTrackerService;
import org.chromium.content_public.browser.UiThreadTaskTraits;

/**
 * This receiver is notified when accounts are added, accounts are removed, or
 * an account's credentials (saved password, etc) are changed.
 * All public methods must be called from the UI thread.
 *
 * TODO(crbug/1193412): We can trigger the signed-in account validation directly
 * after the seeding to avoid listening to the same LOGIN_ACCOUNTS_CHANGED_ACTION
 * event in two different places.
 */
public class AccountsChangedReceiver extends BroadcastReceiver {
    @Override
    public void onReceive(Context context, final Intent intent) {
        if (!AccountManager.LOGIN_ACCOUNTS_CHANGED_ACTION.equals(intent.getAction())) return;

        boolean isChromeVisible = ApplicationStatus.hasVisibleActivities();
        if (isChromeVisible) {
            startBrowserIfNeededAndValidateAccounts();
        }
    }

    private static void startBrowserIfNeededAndValidateAccounts() {
        BrowserParts parts = new EmptyBrowserParts() {
            @Override
            public void finishNativeInitialization() {
                PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
                    AccountTrackerService trackerService =
                            IdentityServicesProvider.get().getAccountTrackerService(
                                    Profile.getLastUsedRegularProfile());
                    trackerService.onAccountsChanged();
                    SigninHelperProvider.get().validateAccountSettings();
                });
            }
        };
        ChromeBrowserInitializer.getInstance().handlePreNativeStartupAndLoadLibraries(parts);
        ChromeBrowserInitializer.getInstance().handlePostNativeStartup(true, parts);
    }
}
