// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.app.Activity;
import android.os.Bundle;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.BuildInfo;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.sync.settings.ManageSyncSettings;
import org.chromium.chrome.browser.ui.signin.SyncConsentDelegate;
import org.chromium.chrome.browser.ui.signin.SyncConsentFragmentBase;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.signin.metrics.SigninAccessPoint;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Implementation of {@link SyncConsentFragmentBase} for {@link SyncConsentActivity}. */
public class SyncConsentFragment extends SyncConsentFragmentBase {
    private static final String ARGUMENT_PERSONALIZED_PROMO_ACTION =
            "SyncConsentFragment.PersonalizedPromoAction";

    @IntDef({
        PromoAction.NONE,
        PromoAction.WITH_DEFAULT,
        PromoAction.NOT_DEFAULT,
        PromoAction.NEW_ACCOUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface PromoAction {
        int NONE = 0;
        int WITH_DEFAULT = 1;
        int NOT_DEFAULT = 2;
        int NEW_ACCOUNT = 3;
    }

    private @PromoAction int mPromoAction;

    /**
     * Creates an argument bundle to start sign-in from personalized sign-in promo.
     * @param accessPoint The access point for starting sign-in flow.
     * @param accountName The account to preselect or null to preselect the default account.
     */
    public static Bundle createArgumentsForPromoDefaultFlow(
            @SigninAccessPoint int accessPoint, String accountName) {
        Bundle result = SyncConsentFragmentBase.createArguments(accessPoint, accountName);
        result.putInt(ARGUMENT_PERSONALIZED_PROMO_ACTION, PromoAction.WITH_DEFAULT);
        return result;
    }

    /**
     * Creates an argument bundle to start "Choose account" sign-in flow from personalized sign-in
     * promo.
     * @param accessPoint The access point for starting sign-in flow.
     * @param accountName The account to preselect or null to preselect the default account.
     */
    public static Bundle createArgumentsForPromoChooseAccountFlow(
            @SigninAccessPoint int accessPoint, String accountName) {
        Bundle result =
                SyncConsentFragmentBase.createArgumentsForChooseAccountFlow(
                        accessPoint, accountName);
        result.putInt(ARGUMENT_PERSONALIZED_PROMO_ACTION, PromoAction.NOT_DEFAULT);
        return result;
    }

    /**
     * Creates an argument bundle to start "New account" sign-in flow from personalized sign-in
     * promo.
     * @param accessPoint The access point for starting sign-in flow.
     */
    public static Bundle createArgumentsForPromoAddAccountFlow(@SigninAccessPoint int accessPoint) {
        Bundle result = SyncConsentFragmentBase.createArgumentsForAddAccountFlow(accessPoint);
        result.putInt(ARGUMENT_PERSONALIZED_PROMO_ACTION, PromoAction.NEW_ACCOUNT);
        return result;
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mPromoAction = getArguments().getInt(ARGUMENT_PERSONALIZED_PROMO_ACTION, PromoAction.NONE);
        recordSigninStartedHistogramAccountInfo();
    }

    @Override
    protected void onSyncRefused() {
        getActivity().finish();
    }

    @Override
    protected void onSyncAccepted(
            String accountName, boolean settingsClicked, SigninManager.SignInCallback callback) {
        signinAndEnableSync(accountName, settingsClicked, callback);
    }

    @Override
    protected void closeAndMaybeOpenSyncSettings(boolean settingsClicked) {
        if (settingsClicked) {
            SettingsNavigation settingsNavigation =
                    SettingsNavigationFactory.createSettingsNavigation();
            settingsNavigation.startSettings(
                    getActivity(),
                    ManageSyncSettings.class,
                    ManageSyncSettings.createArguments(true));
        }

        recordSigninCompletedHistogramAccountInfo();

        Activity activity = getActivity();
        if (activity != null) activity.finish();
    }

    private void recordSigninCompletedHistogramAccountInfo() {
        final String histogram;
        switch (mPromoAction) {
            case PromoAction.NONE:
                return;
            case PromoAction.WITH_DEFAULT:
                histogram = "Signin.SigninCompletedAccessPoint.WithDefault";
                break;
            case PromoAction.NOT_DEFAULT:
                histogram = "Signin.SigninCompletedAccessPoint.NotDefault";
                break;
            case PromoAction.NEW_ACCOUNT:
                // On Android, the promo does not have a button to add an account when there is
                // already an account on the device. That flow goes through the NotDefault promo
                // instead. Always use the NoExistingAccount variant.
                histogram = "Signin.SigninCompletedAccessPoint.NewAccountNoExistingAccount";
                break;
            default:
                assert false : "Unexpected sign-in flow type!";
                return;
        }

        RecordHistogram.recordEnumeratedHistogram(
                histogram, mSigninAccessPoint, SigninAccessPoint.MAX);
    }

    private void recordSigninStartedHistogramAccountInfo() {
        final String histogram;
        switch (mPromoAction) {
            case PromoAction.NONE:
                return;
            case PromoAction.WITH_DEFAULT:
                histogram = "Signin.SigninStartedAccessPoint.WithDefault";
                break;
            case PromoAction.NOT_DEFAULT:
                histogram = "Signin.SigninStartedAccessPoint.NotDefault";
                break;
            case PromoAction.NEW_ACCOUNT:
                // On Android, the promo does not have a button to add an account when there is
                // already an account on the device. That flow goes through the NotDefault promo
                // instead. Always use the NoExistingAccount variant.
                histogram = "Signin.SigninStartedAccessPoint.NewAccountNoExistingAccount";
                break;
            default:
                assert false : "Unexpected sign-in flow type!";
                return;
        }

        RecordHistogram.recordEnumeratedHistogram(
                histogram, mSigninAccessPoint, SigninAccessPoint.MAX);
    }

    @Override
    protected void onAcceptButtonClicked(View button) {
        if (BuildInfo.getInstance().isAutomotive) {
            super.displayDeviceLockPage(() -> super.onAcceptButtonClicked(button));
            return;
        }
        super.onAcceptButtonClicked(button);
    }

    @Override
    protected void onSettingsLinkClicked(View button) {
        if (BuildInfo.getInstance().isAutomotive) {
            super.displayDeviceLockPage(() -> super.onSettingsLinkClicked(button));
            return;
        }
        super.onSettingsLinkClicked(button);
    }

    @Override
    protected SyncConsentDelegate getDelegate() {
        return (SyncConsentDelegate) getActivity();
    }
}
