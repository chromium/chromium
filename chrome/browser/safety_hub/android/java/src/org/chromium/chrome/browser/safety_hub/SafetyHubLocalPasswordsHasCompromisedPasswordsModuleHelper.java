// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.recordDashboardInteractions;

import android.content.Context;
import android.view.View;

import org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.DashboardInteractions;
import org.chromium.chrome.browser.safety_hub.SafetyHubModuleMediator.ModuleState;

/** Helper for the {@link SafetyHubLocalPasswordsModule} for the has compromised passwords state. */
public class SafetyHubLocalPasswordsHasCompromisedPasswordsModuleHelper
        implements SafetyHubModuleHelper {
    private final Context mContext;
    private final SafetyHubModuleDelegate mModuleDelegate;
    private final int mCompromisedPasswordsCount;

    SafetyHubLocalPasswordsHasCompromisedPasswordsModuleHelper(
            Context context,
            SafetyHubModuleDelegate moduleDelegate,
            int compromisedPasswordsCount) {
        mContext = context;
        mModuleDelegate = moduleDelegate;
        mCompromisedPasswordsCount = compromisedPasswordsCount;
        assert mCompromisedPasswordsCount > 0
                : "Compromised passwords count is not greater than zero in"
                        + " HasCompromisedPasswordsPreferenceHelper";
    }

    @Override
    public String getTitle() {
        return mContext.getResources()
                .getQuantityString(
                        R.plurals.safety_hub_local_passwords_compromised_title,
                        mCompromisedPasswordsCount,
                        mCompromisedPasswordsCount);
    }

    @Override
    public String getSummary() {
        return mContext.getResources()
                .getQuantityString(
                        R.plurals.safety_hub_compromised_passwords_summary,
                        mCompromisedPasswordsCount,
                        mCompromisedPasswordsCount);
    }

    @Override
    public String getPrimaryButtonText() {
        return mContext.getString(R.string.safety_hub_passwords_navigation_button);
    }

    @Override
    public View.OnClickListener getPrimaryButtonListener() {
        return v -> {
            mModuleDelegate.showLocalPasswordCheckUi(mContext);
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
        return ModuleState.WARNING;
    }
}
