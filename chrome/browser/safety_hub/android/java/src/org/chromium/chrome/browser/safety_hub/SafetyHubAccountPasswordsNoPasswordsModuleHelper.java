// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.recordDashboardInteractions;

import android.content.Context;
import android.view.View;

import org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.DashboardInteractions;
import org.chromium.chrome.browser.safety_hub.SafetyHubModuleMediator.ModuleState;

/** Helper for the {@link SafetyHubAccountPasswordsModule} for the has no passwords state. */
public class SafetyHubAccountPasswordsNoPasswordsModuleHelper implements SafetyHubModuleHelper {
    private final Context mContext;
    private final SafetyHubModuleDelegate mModuleDelegate;

    SafetyHubAccountPasswordsNoPasswordsModuleHelper(
            Context context, SafetyHubModuleDelegate moduleDelegate) {
        mContext = context;
        mModuleDelegate = moduleDelegate;
    }

    @Override
    public String getTitle() {
        return mContext.getString(R.string.safety_hub_no_passwords_title);
    }

    @Override
    public String getSummary() {
        return mContext.getString(R.string.safety_hub_no_passwords_summary);
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
        return ModuleState.INFO;
    }
}
