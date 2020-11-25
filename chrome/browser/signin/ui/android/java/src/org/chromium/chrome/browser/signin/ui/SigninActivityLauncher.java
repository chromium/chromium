// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.ui;

import android.content.Context;

import org.chromium.components.signin.metrics.SigninAccessPoint;

/** Allows for launching {@link SigninActivity} in modularized code. */
public interface SigninActivityLauncher {
    /**
     * Launches the SigninActivity with default sign-in flow from personalized sign-in promo.
     * @param accessPoint {@link SigninAccessPoint} for starting sign-in flow.
     * @param accountName The account to preselect or null to preselect the default account.
     */
    void launchActivityForPromoDefaultFlow(
            Context context, @SigninAccessPoint int accessPoint, String accountName);

    /**
     * Launches the SigninActivity with "Choose account" sign-in flow from personalized
     * sign-in promo.
     * @param accessPoint {@link SigninAccessPoint} for starting sign-in flow.
     * @param accountName The account to preselect or null to preselect the default account.
     */
    void launchActivityForPromoChooseAccountFlow(
            Context context, @SigninAccessPoint int accessPoint, String accountName);

    /**
     * Launches the SigninActivity with "New account" sign-in flow from personalized sign-in
     * promo.
     * @param accessPoint {@link SigninAccessPoint} for starting sign-in flow.
     */
    void launchActivityForPromoAddAccountFlow(Context context, @SigninAccessPoint int accessPoint);

    /**
     * Launches a {@link SigninActivity}.
     * @param context A {@link Context} object.
     * @param accessPoint {@link SigninAccessPoint} enum value representing.
     */
    void launchActivity(Context context, @SigninAccessPoint int accessPoint);
}
