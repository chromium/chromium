// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.accounts.Account;
import android.content.Context;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp.cards.SignInPromo;
import org.chromium.chrome.browser.signin.services.FREMobileIdentityConsistencyFieldTrial;
import org.chromium.chrome.browser.ui.signin.SyncConsentFragmentBase;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.metrics.SigninAccessPoint;

import java.util.List;

/**
 * Implementation of {@link SyncConsentFragmentBase} for the first run experience.
 */
public class SyncConsentFirstRunFragment
        extends SyncConsentFragmentBase implements FirstRunFragment {
    // Per-page parameters:
    // TODO(crbug/1168516): Remove IS_CHILD_ACCOUNT
    public static final String IS_CHILD_ACCOUNT = "IsChildAccount";

    // Do not remove. Empty fragment constructor is required for re-creating the fragment from a
    // saved state bundle. See crbug.com/1225102
    public SyncConsentFirstRunFragment() {}

    /**
     * Returns true if sync will be enabled as soon as the user clicks the "Yes, I'm in" button,
     * and false if this will be deferred until the main activity starts.
     */
    public static boolean shouldEnableImmediately() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.ALLOW_SYNC_OFF_FOR_CHILD_ACCOUNTS)
                && ChromeFeatureList.isEnabled(ChromeFeatureList.ENABLE_SYNC_IMMEDIATELY_IN_FRE);
    }

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        final List<Account> accounts = AccountUtils.getAccountsIfFulfilledOrEmpty(
                AccountManagerFacadeProvider.getInstance().getAccounts());
        boolean isChild = getPageDelegate().getProperties().getBoolean(IS_CHILD_ACCOUNT, false);
        setArguments(createArguments(SigninAccessPoint.START_PAGE,
                accounts.isEmpty() ? null : accounts.get(0).name, isChild));
    }

    @Override
    protected void onSyncRefused() {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.ALLOW_SYNC_OFF_FOR_CHILD_ACCOUNTS)
                && mIsChild) {
            // Somehow the child account disappeared while we were in the FRE.
            // The user would have to go through the FRE again.
            getPageDelegate().abortFirstRunExperience();
        } else {
            SignInPromo.temporarilySuppressPromos();
            FirstRunSignInProcessor.setFirstRunFlowSignInAccountName(null);
            FirstRunSignInProcessor.setFirstRunFlowSignInSetup(false);
            getPageDelegate().recordFreProgressHistogram(MobileFreProgress.SYNC_CONSENT_DISMISSED);
            getPageDelegate().advanceToNextPage();
        }
    }

    @Override
    protected void onSyncAccepted(String accountName, boolean settingsClicked, Runnable callback) {
        // TODO(crbug.com/1302635): Once ENABLE_SYNC_IMMEDIATELY_IN_FRE launches, move these metrics
        // elsewhere, so onSyncAccepted() is replaced with signinAndEnableSync() (common code).
        getPageDelegate().recordFreProgressHistogram(MobileFreProgress.SYNC_CONSENT_ACCEPTED);
        if (settingsClicked) {
            getPageDelegate().recordFreProgressHistogram(
                    MobileFreProgress.SYNC_CONSENT_SETTINGS_LINK_CLICK);
        }

        if (shouldEnableImmediately()) {
            // Enable sync now. Leave the account pref empty in FirstRunSignInProcessor, so start()
            // doesn't try to do it a second time. Only set the advanced setup pref later in
            // closeAndMaybeOpenSyncSettings(), because settings shouldn't open if
            // signinAndEnableSync() fails.
            FirstRunSignInProcessor.setFirstRunFlowSignInAccountName(null);
            signinAndEnableSync(accountName, settingsClicked, callback);
        } else {
            // Enabling sync is deferred to FirstRunSignInProcessor.start().
            FirstRunSignInProcessor.setFirstRunFlowSignInAccountName(accountName);
            FirstRunSignInProcessor.setFirstRunFlowSignInSetup(settingsClicked);
            getPageDelegate().advanceToNextPage();
            callback.run();
        }
    }

    @Override
    protected void closeAndMaybeOpenSyncSettings(boolean settingsClicked) {
        assert shouldEnableImmediately();
        // Now that signinAndEnableSync() succeeded, signal whether FirstRunSignInProcessor.start()
        // should open settings.
        FirstRunSignInProcessor.setFirstRunFlowSignInSetup(settingsClicked);
        getPageDelegate().advanceToNextPage();
    }

    @Override
    public void setInitialA11yFocus() {
        // Ignore calls before view is created.
        if (getView() == null) return;

        final View title = getView().findViewById(R.id.signin_title);
        title.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
    }

    @Override
    protected void updateAccounts(List<Account> accounts) {
        final boolean selectedAccountDoesNotExist = (mSelectedAccountName != null
                && AccountUtils.findAccountByName(accounts, mSelectedAccountName) == null);
        if (FREMobileIdentityConsistencyFieldTrial.isEnabled() && selectedAccountDoesNotExist) {
            // With MICe, there's no account picker and the sync consent is fixed for the signed
            // in account on welcome screen. If the signed-in account is removed, this page
            // no longer makes sense, so we abort the FRE here to allow users to restart FRE.
            getPageDelegate().abortFirstRunExperience();
            return;
        }
        super.updateAccounts(accounts);
    }
}
