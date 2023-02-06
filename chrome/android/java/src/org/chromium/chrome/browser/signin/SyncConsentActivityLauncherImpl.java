// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.signin;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.ui.signin.SyncConsentActivityLauncher;
import org.chromium.chrome.browser.ui.signin.SyncConsentFragmentBase;
import org.chromium.components.browser_ui.settings.ManagedPreferencesUtils;
import org.chromium.components.signin.metrics.SigninAccessPoint;

/**
 * SyncConsentActivityLauncher creates the proper intent and then launches the
 * {@link SyncConsentActivity} in different scenarios.
 */
public final class SyncConsentActivityLauncherImpl implements SyncConsentActivityLauncher {
    private static SyncConsentActivityLauncher sLauncher;

    /**
     * Singleton instance getter
     */
    public static SyncConsentActivityLauncher get() {
        if (sLauncher == null) {
            sLauncher = new SyncConsentActivityLauncherImpl();
        }
        return sLauncher;
    }

    @VisibleForTesting
    public static void setLauncherForTest(@Nullable SyncConsentActivityLauncher launcher) {
        sLauncher = launcher;
    }

    private SyncConsentActivityLauncherImpl() {}

    /**
     * Launches the {@link SyncConsentActivity} with default sign-in flow from personalized sign-in
     * promo.
     * @param accessPoint {@link SigninAccessPoint} for starting sign-in flow.
     * @param accountName The account to preselect or null to preselect the default account.
     */
    @Override
    public void launchActivityForPromoDefaultFlow(
            Context context, @SigninAccessPoint int accessPoint, String accountName) {
        launchInternal(context,
                SyncConsentFragment.createArgumentsForPromoDefaultFlow(accessPoint, accountName));
    }

    /**
     * Launches the {@link SyncConsentActivity} with "Choose account" sign-in flow from personalized
     * sign-in promo.
     * @param accessPoint {@link SigninAccessPoint} for starting sign-in flow.
     * @param accountName The account to preselect or null to preselect the default account.
     */
    @Override
    public void launchActivityForPromoChooseAccountFlow(
            Context context, @SigninAccessPoint int accessPoint, String accountName) {
        launchInternal(context,
                SyncConsentFragment.createArgumentsForPromoChooseAccountFlow(
                        accessPoint, accountName));
    }

    /**
     * Launches the {@link SyncConsentActivity} with "New account" sign-in flow from personalized
     * sign-in promo.
     * @param accessPoint {@link SigninAccessPoint} for starting sign-in flow.
     */
    @Override
    public void launchActivityForPromoAddAccountFlow(
            Context context, @SigninAccessPoint int accessPoint) {
        launchInternal(
                context, SyncConsentFragment.createArgumentsForPromoAddAccountFlow(accessPoint));
    }

    /**
     * Launches the {@link SyncConsentActivity} if signin is allowed.
     * @param context A {@link Context} object.
     * @param accessPoint {@link SigninAccessPoint} for starting sign-in flow.
     * @return a boolean indicating if the {@link SyncConsentActivity} is launched.
     */
    @Override
    public boolean launchActivityIfAllowed(Context context, @SigninAccessPoint int accessPoint) {
        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(
                Profile.getLastUsedRegularProfile());
        if (signinManager.isSyncOptInAllowed()) {
            launchInternal(context, SyncConsentFragmentBase.createArguments(accessPoint, null));
            return true;
        }
        if (signinManager.isSigninDisabledByPolicy()) {
            ManagedPreferencesUtils.showManagedByAdministratorToast(context);
        }
        return false;
    }

    /**
     * Launches the {@link SyncConsentActivity} with Tangible Sync flow.
     * @param accessPoint {@link SigninAccessPoint} for starting sign-in flow.
     * @param accountName The account to select.
     */
    @Override
    public void launchActivityForTangibleSyncFlow(
            Context context, @SigninAccessPoint int accessPoint, String accountName) {
        launchInternal(context,
                SyncConsentFragmentBase.createArgumentsForTangibleSync(accessPoint, accountName));
    }

    /**
     * Launches the {@link SyncConsentActivity} with "New Account" sign-in flow for Tangible Sync.
     * @param accessPoint {@link SigninAccessPoint} for starting sign-in flow.
     */
    @Override
    public void launchActivityForTangibleSyncAddAccountFlow(
            Context context, @SigninAccessPoint int accessPoint) {
        launchInternal(context,
                SyncConsentFragmentBase.createArgumentsForTangibleSyncAddAccountFlow(accessPoint));
    }

    private void launchInternal(Context context, Bundle fragmentArgs) {
        Intent intent = SyncConsentActivity.createIntent(context, fragmentArgs);
        context.startActivity(intent);
    }
}
