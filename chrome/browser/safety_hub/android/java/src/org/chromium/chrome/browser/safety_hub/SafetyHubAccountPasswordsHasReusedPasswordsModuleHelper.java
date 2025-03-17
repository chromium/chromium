// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.recordDashboardInteractions;

import android.content.Context;
import android.view.View;

import org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.DashboardInteractions;
import org.chromium.chrome.browser.safety_hub.SafetyHubModuleMediator.ModuleState;

/** Helper for the {@link SafetyHubAccountPasswordsModule} for the has reused passwords state. */
public class SafetyHubAccountPasswordsHasReusedPasswordsModuleHelper
        implements SafetyHubModuleHelper {
    private final Context mContext;
    private final SafetyHubModuleDelegate mModuleDelegate;
    private final int mReusedPasswordsCount;

    SafetyHubAccountPasswordsHasReusedPasswordsModuleHelper(
            Context context, SafetyHubModuleDelegate moduleDelegate, int reusedPasswordsCount) {
        mContext = context;
        mModuleDelegate = moduleDelegate;
        mReusedPasswordsCount = reusedPasswordsCount;
        assert mReusedPasswordsCount > 0
                : "Reused passwords count is not greater than zero in"
                        + " HasReusedPasswordsPreferenceHelper";
    }

    @Override
    public String getTitle() {
        return mContext.getString(R.string.safety_hub_reused_weak_passwords_title);
    }

    @Override
    public String getSummary() {
        return mContext.getResources()
                .getQuantityString(
                        R.plurals.safety_hub_reused_passwords_summary,
                        mReusedPasswordsCount,
                        mReusedPasswordsCount);
    }

    @Override
    public String getPrimaryButtonText() {
        return mContext.getString(R.string.safety_hub_passwords_navigation_button);
    }

    @Override
    public View.OnClickListener getPrimaryButtonListener() {
        return v -> {
            mModuleDelegate.showPasswordCheckUi(mContext);
            recordDashboardInteractions(DashboardInteractions.OPEN_PASSWORD_MANAGER);
        };
    }

    @Override
    public String getSecondaryButtonText() {
        return null;
    }

    @Override
    public View.OnClickListener getSecondaryButtonListener() {
        return null;
    }

    @Override
    public @ModuleState int getModuleState() {
        return ModuleState.INFO;
    }
}
