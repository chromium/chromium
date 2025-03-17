// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.recordDashboardInteractions;

import android.content.Context;
import android.view.View;

import org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.DashboardInteractions;
import org.chromium.chrome.browser.safety_hub.SafetyHubModuleMediator.ModuleState;

/**
 * Helper for the {@link SafetyHubAccountPasswordsModule} for the has no compromised passwords
 * state.
 */
public class SafetyHubAccountPasswordsNoCompromisedPasswordsModuleHelper
        implements SafetyHubModuleHelper {
    private final Context mContext;
    private final SafetyHubModuleDelegate mModuleDelegate;
    private final String mAccount;

    SafetyHubAccountPasswordsNoCompromisedPasswordsModuleHelper(
            Context context, SafetyHubModuleDelegate moduleDelegate, String account) {
        mContext = context;
        mModuleDelegate = moduleDelegate;
        mAccount = account;
    }

    @Override
    public String getTitle() {
        return mContext.getString(R.string.safety_hub_no_compromised_passwords_title);
    }

    @Override
    public String getSummary() {
        if (mAccount != null) {
            return mContext.getString(R.string.safety_hub_password_check_time_recently, mAccount);
        } else {
            return mContext.getString(R.string.safety_hub_checked_recently);
        }
    }

    @Override
    public String getPrimaryButtonText() {
        return null;
    }

    @Override
    public View.OnClickListener getPrimaryButtonListener() {
        return null;
    }

    @Override
    public String getSecondaryButtonText() {
        return mContext.getString(R.string.safety_hub_passwords_navigation_button);
    }

    @Override
    public View.OnClickListener getSecondaryButtonListener() {
        return v -> {
            mModuleDelegate.showPasswordCheckUi(mContext);
            recordDashboardInteractions(DashboardInteractions.OPEN_PASSWORD_MANAGER);
        };
    }

    @Override
    public @ModuleState int getModuleState() {
        return ModuleState.SAFE;
    }
}
