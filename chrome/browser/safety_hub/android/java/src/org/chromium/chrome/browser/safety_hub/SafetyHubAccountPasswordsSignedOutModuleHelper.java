// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.recordDashboardInteractions;

import android.content.Context;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.DashboardInteractions;
import org.chromium.chrome.browser.safety_hub.SafetyHubModuleMediator.ModuleState;

/** Helper for the {@link SafetyHubAccountPasswordsModule} for the not signed in state. */
@NullMarked
public class SafetyHubAccountPasswordsSignedOutModuleHelper implements SafetyHubModuleHelper {
    private final Context mContext;
    private final SafetyHubModuleDelegate mModuleDelegate;

    SafetyHubAccountPasswordsSignedOutModuleHelper(
            Context context, SafetyHubModuleDelegate moduleDelegate) {
        mContext = context;
        mModuleDelegate = moduleDelegate;
    }

    @Override
    public String getTitle() {
        return mContext.getString(R.string.safety_hub_account_password_check_unavailable_title);
    }

    @Override
    public String getSummary() {
        return mContext.getString(R.string.safety_hub_password_check_signed_out_summary);
    }

    @Override
    public @Nullable String getPrimaryButtonText() {
        return null;
    }

    @Override
    public View.@Nullable OnClickListener getPrimaryButtonListener() {
        return null;
    }

    @Override
    public String getSecondaryButtonText() {
        return mContext.getString(R.string.sign_in_to_chrome);
    }

    @Override
    public View.OnClickListener getSecondaryButtonListener() {
        return v -> {
            mModuleDelegate.launchSigninPromo(mContext);
            recordDashboardInteractions(DashboardInteractions.SHOW_SIGN_IN_PROMO);
        };
    }

    @Override
    public @ModuleState int getModuleState() {
        return ModuleState.UNAVAILABLE;
    }
}
