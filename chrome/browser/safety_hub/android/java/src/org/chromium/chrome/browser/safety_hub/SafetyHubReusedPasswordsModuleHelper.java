// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.recordDashboardInteractions;

import android.content.Context;
import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.DashboardInteractions;
import org.chromium.chrome.browser.safety_hub.SafetyHubModuleMediator.ModuleState;

/** Helper for the {@link SafetyHubPasswordsModule} for the has reused passwords state. */
@NullMarked
public class SafetyHubReusedPasswordsModuleHelper implements SafetyHubModuleHelper {
    private final Context mContext;
    private final SafetyHubModuleDelegate mModuleDelegate;
    private final int mAccountReusedPasswordsCount;
    private final int mLocalReusedPasswordsCount;
    private final boolean mUnifiedModule;

    SafetyHubReusedPasswordsModuleHelper(
            Context context,
            SafetyHubModuleDelegate moduleDelegate,
            int accountReusedPasswordsCount,
            int localReusedPasswordsCount,
            boolean unifiedModule) {
        mContext = context;
        mModuleDelegate = moduleDelegate;
        mAccountReusedPasswordsCount = accountReusedPasswordsCount;
        mLocalReusedPasswordsCount = localReusedPasswordsCount;
        mUnifiedModule = unifiedModule;
        assert mAccountReusedPasswordsCount > 0 || mLocalReusedPasswordsCount > 0
                : "Reused passwords count is not greater than zero in ReusedPasswordsModuleHelper";
    }

    @Override
    public String getTitle() {
        if (mAccountReusedPasswordsCount > 0 && mLocalReusedPasswordsCount > 0) {
            return mContext.getString(R.string.safety_hub_reused_weak_passwords_title);
        }
        if (mAccountReusedPasswordsCount > 0) {
            return mContext.getString(R.string.safety_hub_reused_weak_account_passwords_title);
        }
        return mContext.getString(R.string.safety_hub_reused_weak_local_passwords_title);
    }

    @Override
    public String getSummary() {
        int totalReusedPasswordsCount = mAccountReusedPasswordsCount + mLocalReusedPasswordsCount;
        return mContext.getResources()
                .getQuantityString(
                        R.plurals.safety_hub_reused_passwords_summary,
                        totalReusedPasswordsCount,
                        totalReusedPasswordsCount);
    }

    @Override
    public String getPrimaryButtonText() {
        if (mAccountReusedPasswordsCount > 0 && mLocalReusedPasswordsCount > 0) {
            return mContext.getString(R.string.safety_hub_password_subpage_navigation_button);
        }
        return mContext.getString(R.string.safety_hub_passwords_navigation_button);
    }

    @Override
    public View.OnClickListener getPrimaryButtonListener() {
        if (mAccountReusedPasswordsCount > 0 && mLocalReusedPasswordsCount > 0) {
            return v -> {
                // TODO(crbug.com/407931779): Change to open the SH passwords page.
                mModuleDelegate.showLocalPasswordCheckUi(mContext);
            };
        }
        if (mAccountReusedPasswordsCount > 0) {
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
                && !(mAccountReusedPasswordsCount > 0 && mLocalReusedPasswordsCount > 0)) {
            return mContext.getString(R.string.safety_hub_password_subpage_navigation_button);
        }
        return null;
    }

    @Override
    public @Nullable View.OnClickListener getSecondaryButtonListener() {
        if (mUnifiedModule
                && !(mAccountReusedPasswordsCount > 0 && mLocalReusedPasswordsCount > 0)) {
            return v -> {
                // TODO(crbug.com/407931779): Change to open the SH passwords page.
                mModuleDelegate.showLocalPasswordCheckUi(mContext);
            };
        }
        return null;
    }

    @Override
    public @ModuleState int getModuleState() {
        return ModuleState.INFO;
    }
}
