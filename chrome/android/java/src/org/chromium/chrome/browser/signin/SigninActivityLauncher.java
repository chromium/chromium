// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.signin;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.components.signin.metrics.SigninAccessPoint;

/**
 * SigninActivityLauncher creates the proper intent and then launches the
 * SigninActivity in different scenarios.
 */
public class SigninActivityLauncher {
    private static SigninActivityLauncher sLauncher;

    /**
     * Singleton instance getter
     */
    public static SigninActivityLauncher get() {
        if (sLauncher == null) {
            sLauncher = new SigninActivityLauncher();
        }
        return sLauncher;
    }

    @VisibleForTesting
    public static void setLauncherForTest(@Nullable SigninActivityLauncher launcher) {
        sLauncher = launcher;
    }

    private SigninActivityLauncher() {}

    /**
     * Launch the SigninActivity with default sign-in flow from personalized sign-in promo.
     * @param accessPoint {@link SigninAccessPoint} for starting sign-in flow.
     * @param accountName The account to preselect or null to preselect the default account.
     */
    public void launchActivityForPromoDefaultFlow(
            Context context, @SigninAccessPoint int accessPoint, String accountName) {
        launchInternal(context,
                SigninFragment.createArgumentsForPromoDefaultFlow(accessPoint, accountName));
    }

    /**
     * Launch the SigninActivity with "Choose account" sign-in flow from personalized
     * sign-in promo.
     * @param accessPoint {@link SigninAccessPoint} for starting sign-in flow.
     * @param accountName The account to preselect or null to preselect the default account.
     */
    public void launchActivityForPromoChooseAccountFlow(
            Context context, @SigninAccessPoint int accessPoint, String accountName) {
        launchInternal(context,
                SigninFragment.createArgumentsForPromoChooseAccountFlow(accessPoint, accountName));
    }

    /**
     * Launch the SigninActivity with "New account" sign-in flow from personalized sign-in
     * promo.
     * @param accessPoint {@link SigninAccessPoint} for starting sign-in flow.
     */
    public void launchActivityForPromoAddAccountFlow(
            Context context, @SigninAccessPoint int accessPoint) {
        launchInternal(context, SigninFragment.createArgumentsForPromoAddAccountFlow(accessPoint));
    }

    /**
     * Creates an {@link Intent} which can be used to start sign-in flow and launch the signin
     * activity.
     * @param accessPoint {@link SigninAccessPoint} for starting sign-in flow.
     */
    public void launchActivity(Context context, @SigninAccessPoint int accessPoint) {
        launchInternal(context, SigninFragment.createArguments(accessPoint));
    }

    private void launchInternal(Context context, Bundle fragmentArgs) {
        Intent intent = new Intent(context, SigninActivity.class);
        intent.putExtra(SigninActivity.ARGUMENT_FRAGMENT_ARGS, fragmentArgs);
        context.startActivity(intent);
    }
}
