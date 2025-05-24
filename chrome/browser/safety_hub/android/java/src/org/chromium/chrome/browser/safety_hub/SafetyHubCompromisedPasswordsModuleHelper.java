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

/** Helper for the {@link SafetyHubPasswordsModule} for the has compromised passwords state. */
@NullMarked
public class SafetyHubCompromisedPasswordsModuleHelper implements SafetyHubModuleHelper {
    private final Context mContext;
    private final SafetyHubModuleDelegate mModuleDelegate;
    private final int mAccountCompromisedPasswordsCount;
    private final int mLocalCompromisedPasswordsCount;
    private final boolean mUnifiedModule;

    SafetyHubCompromisedPasswordsModuleHelper(
            Context context,
            SafetyHubModuleDelegate moduleDelegate,
            int accountCompromisedPasswordsCount,
            int localCompromisedPasswordsCount,
            boolean unifiedModule) {
        mContext = context;
        mModuleDelegate = moduleDelegate;
        mAccountCompromisedPasswordsCount = accountCompromisedPasswordsCount;
        mLocalCompromisedPasswordsCount = localCompromisedPasswordsCount;
        mUnifiedModule = unifiedModule;
        assert mAccountCompromisedPasswordsCount > 0 || mLocalCompromisedPasswordsCount > 0
                : "Compromised passwords count is not greater than zero in"
                        + "CompromisedPasswordsModuleHelper";
    }

    @Override
    public String getTitle() {
        if (mAccountCompromisedPasswordsCount > 0 && mLocalCompromisedPasswordsCount > 0) {
            int totalCompromisedPasswordsCount =
                    mAccountCompromisedPasswordsCount + mLocalCompromisedPasswordsCount;
            return mContext.getResources()
                    .getQuantityString(
                            R.plurals.safety_hub_passwords_compromised_title,
                            totalCompromisedPasswordsCount,
                            totalCompromisedPasswordsCount);
        }
        if (mAccountCompromisedPasswordsCount > 0) {
            return mContext.getResources()
                    .getQuantityString(
                            R.plurals.safety_hub_account_passwords_compromised_exist,
                            mAccountCompromisedPasswordsCount,
                            mAccountCompromisedPasswordsCount);
        }
        return mContext.getResources()
                .getQuantityString(
                        R.plurals.safety_hub_local_passwords_compromised_title,
                        mLocalCompromisedPasswordsCount,
                        mLocalCompromisedPasswordsCount);
    }

    @Override
    public String getSummary() {
        int totalCompromisedPasswordsCount =
                mAccountCompromisedPasswordsCount + mLocalCompromisedPasswordsCount;
        return mContext.getResources()
                .getQuantityString(
                        R.plurals.safety_hub_compromised_passwords_summary,
                        totalCompromisedPasswordsCount,
                        totalCompromisedPasswordsCount);
    }

    @Override
    public String getPrimaryButtonText() {
        if (mAccountCompromisedPasswordsCount > 0 && mLocalCompromisedPasswordsCount > 0) {
            return mContext.getString(R.string.safety_hub_password_subpage_navigation_button);
        }
        return mContext.getString(R.string.safety_hub_passwords_navigation_button);
    }

    @Override
    public View.OnClickListener getPrimaryButtonListener() {
        if (mAccountCompromisedPasswordsCount > 0 && mLocalCompromisedPasswordsCount > 0) {
            return v -> {
                // TODO(crbug.com/407931779): Change to open the SH passwords page.
                mModuleDelegate.showLocalPasswordCheckUi(mContext);
            };
        }
        if (mAccountCompromisedPasswordsCount > 0) {
            return v -> {
                mModuleDelegate.showPasswordCheckUi(mContext);
                recordDashboardInteractions(DashboardInteractions.OPEN_PASSWORD_MANAGER);
            };
        }
        return v -> {
            mModuleDelegate.showLocalPasswordCheckUi(mContext);
            recordDashboardInteractions(DashboardInteractions.OPEN_PASSWORD_MANAGER);
        };
    }

    @Override
    public @Nullable String getSecondaryButtonText() {
        if (mUnifiedModule
                && !(mAccountCompromisedPasswordsCount > 0
                        && mLocalCompromisedPasswordsCount > 0)) {
            return mContext.getString(R.string.safety_hub_password_subpage_navigation_button);
        }
        return null;
    }

    @Override
    public View.@Nullable OnClickListener getSecondaryButtonListener() {
        if (mUnifiedModule
                && !(mAccountCompromisedPasswordsCount > 0
                        && mLocalCompromisedPasswordsCount > 0)) {
            return v -> {
                // TODO(crbug.com/407931779): Change to open the SH passwords page.
                mModuleDelegate.showLocalPasswordCheckUi(mContext);
            };
        }
        return null;
    }

    @Override
    public @ModuleState int getModuleState() {
        return ModuleState.WARNING;
    }
}
