// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.content.Context;
import android.os.Bundle;
import android.support.v4.app.Fragment;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.cards.SignInPromo;
import org.chromium.chrome.browser.signin.SigninFragmentBase;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.components.signin.ChildAccountStatus;
import org.chromium.components.signin.metrics.SigninAccessPoint;

/** A {@link Fragment} to handle sign-in within the first run experience. */
public class SigninFirstRunFragment extends SigninFragmentBase implements FirstRunFragment {
    // Per-page parameters:
    public static final String FORCE_SIGNIN_ACCOUNT_TO = "ForceSigninAccountTo";
    public static final String CHILD_ACCOUNT_STATUS = "ChildAccountStatus";

    private Bundle mArguments;

    // Every fragment must have a public default constructor.
    public SigninFirstRunFragment() {}

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);

        Bundle freProperties = getPageDelegate().getProperties();
        String forceAccountTo = freProperties.getString(FORCE_SIGNIN_ACCOUNT_TO);
        if (forceAccountTo == null) {
            mArguments = createArguments(null);
        } else {
            @ChildAccountStatus.Status
            int childAccountStatus = freProperties.getInt(CHILD_ACCOUNT_STATUS);
            mArguments = createArgumentsForForcedSigninFlow(forceAccountTo, childAccountStatus);
        }

        RecordUserAction.record("MobileFre.SignInShown");
        RecordUserAction.record("Signin_Signin_FromStartPage");
        SigninManager.logSigninStartAccessPoint(SigninAccessPoint.START_PAGE);
    }

    @Override
    protected Bundle getSigninArguments() {
        return mArguments;
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
}
