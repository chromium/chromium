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

/** Helper for the {@link SafetyHubPasswordsModule} for the no compromised passwords state. */
@NullMarked
public class SafetyHubNoCompromisedPasswordsModuleHelper implements SafetyHubModuleHelper {
    private final Context mContext;
    private final SafetyHubModuleDelegate mModuleDelegate;
    private final @Nullable String mAccount;
    private final boolean mUnifiedModule;

    SafetyHubNoCompromisedPasswordsModuleHelper(
            Context context,
            SafetyHubModuleDelegate moduleDelegate,
            @Nullable String account,
            boolean unifiedModule) {
        mContext = context;
        mModuleDelegate = moduleDelegate;
        mAccount = account;
        mUnifiedModule = unifiedModule;
    }

    @Override
    public String getTitle() {
        if (isAccountModule()) {
            return mContext.getString(R.string.safety_hub_no_compromised_account_passwords_title);
        }

        if (isLocalModule()) {
            return mContext.getString(R.string.safety_hub_no_compromised_local_passwords_title);
        }

        assert mUnifiedModule;
        return mContext.getString(R.string.safety_hub_no_compromised_passwords_title);
    }

    @Override
    public String getSummary() {
        if (isAccountModule()) {
            return mAccount
                    + "\n"
                    + mContext.getString(R.string.safety_hub_no_compromised_passwords_summary);
        }

        return mContext.getString(R.string.safety_hub_no_compromised_passwords_summary);
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
        if (mUnifiedModule) {
            return mContext.getString(R.string.safety_hub_password_subpage_navigation_button);
        }

        return mContext.getString(R.string.safety_hub_passwords_navigation_button);
    }

    @Override
    public View.OnClickListener getSecondaryButtonListener() {
        if (isAccountModule()) {
            return v -> {
                mModuleDelegate.showPasswordCheckUi(mContext);
                recordDashboardInteractions(DashboardInteractions.OPEN_PASSWORD_MANAGER);
            };
        }

        if (isLocalModule()) {
            return v -> {
                mModuleDelegate.showLocalPasswordCheckUi(mContext);
            };
        }

        assert mUnifiedModule;
        return v -> {
            // TODO(crbug.com/407931779): Change to open the SH passwords page.
            mModuleDelegate.showLocalPasswordCheckUi(mContext);
        };
    }

    @Override
    public @ModuleState int getModuleState() {
        return ModuleState.SAFE;
    }

    private boolean isAccountModule() {
        return mAccount != null && !mUnifiedModule;
    }

    private boolean isLocalModule() {
        return mAccount == null && !mUnifiedModule;
    }
}
