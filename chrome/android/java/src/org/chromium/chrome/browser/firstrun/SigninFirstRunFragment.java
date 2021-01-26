// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.accounts.Account;
import android.content.Context;
import android.os.Bundle;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.cards.SignInPromo;
import org.chromium.chrome.browser.signin.SigninFragmentBase;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.ChildAccountStatus;
import org.chromium.components.signin.metrics.SigninAccessPoint;

import java.util.List;

/**
 * Implementation of {@link SigninFragmentBase} for the first run experience.
 */
public class SigninFirstRunFragment extends SigninFragmentBase implements FirstRunFragment {
    // Per-page parameters:
    // TODO(crbug/1168516): Remove CHILD_ACCOUNT_STATUS
    public static final String CHILD_ACCOUNT_STATUS = "ChildAccountStatus";

    // Every fragment must have a public default constructor.
    public SigninFirstRunFragment() {}

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        final List<Account> accounts =
                AccountManagerFacadeProvider.getInstance().tryGetGoogleAccounts();
        final Bundle freProperties = getPageDelegate().getProperties();
        final @ChildAccountStatus.Status int childAccountStatus =
                freProperties.getInt(CHILD_ACCOUNT_STATUS);
        setArguments(ChildAccountStatus.isChild(childAccountStatus)
                        ? createArgumentsForForcedSigninFlow(
                                accounts.get(0).name, childAccountStatus)
                        : createArguments(null));
        // Records if there are {0, 1, 2+} accounts on device for default/non-default flows.
        RecordHistogram.recordCountHistogram(
                "Signin.AndroidDeviceAccountsNumberWhenEnteringFRE", Math.min(accounts.size(), 2));
        RecordUserAction.record("MobileFre.SignInShown");
        SigninMetricsUtils.logSigninStartAccessPoint(SigninAccessPoint.START_PAGE);
        SigninMetricsUtils.logSigninUserActionForAccessPoint(SigninAccessPoint.START_PAGE);
    }

    @Override
    protected void onSigninRefused() {
        if (isForcedSignin()) {
            // Somehow the forced account disappeared while we were in the FRE.
            // The user would have to go through the FRE again.
            getPageDelegate().abortFirstRunExperience();
        } else {
            SignInPromo.temporarilySuppressPromos();
            getPageDelegate().refuseSignIn();
            getPageDelegate().advanceToNextPage();
        }
    }

    @Override
    protected void onSigninAccepted(String accountName, boolean isDefaultAccount,
            boolean settingsClicked, Runnable callback) {
        getPageDelegate().acceptSignIn(accountName, isDefaultAccount, settingsClicked);
        getPageDelegate().advanceToNextPage();
        callback.run();
    }

    @Override
    protected int getNegativeButtonTextId() {
        return R.string.no_thanks;
    }

    @Override
    public void setInitialA11yFocus() {
        // Ignore calls before view is created.
        if (getView() == null) return;

        final View title = getView().findViewById(R.id.signin_title);
        title.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
    }
}
