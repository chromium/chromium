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

/** Helper for the {@link SafetyHubPasswordsModule} for the has weak passwords state. */
@NullMarked
public class SafetyHubWeakPasswordsModuleHelper implements SafetyHubModuleHelper {
    private final Context mContext;
    private final SafetyHubModuleDelegate mModuleDelegate;
    private final int mAccountWeakPasswordsCount;
    private final int mLocalWeakPasswordsCount;
    private final boolean mUnifiedModule;

    SafetyHubWeakPasswordsModuleHelper(
            Context context,
            SafetyHubModuleDelegate moduleDelegate,
            int accountWeakPasswordsCount,
            int localWeakPasswordsCount,
            boolean unifiedModule) {
        mContext = context;
        mModuleDelegate = moduleDelegate;
        mAccountWeakPasswordsCount = accountWeakPasswordsCount;
        mLocalWeakPasswordsCount = localWeakPasswordsCount;
        mUnifiedModule = unifiedModule;
        assert mAccountWeakPasswordsCount > 0 || mLocalWeakPasswordsCount > 0
                : "Weak passwords count is not greater than zero in" + " WeakPasswordsModuleHelper";
    }

    @Override
    public String getTitle() {
        if (mAccountWeakPasswordsCount > 0 && mLocalWeakPasswordsCount > 0) {
            return mContext.getString(R.string.safety_hub_reused_weak_passwords_title);
        }
        if (mAccountWeakPasswordsCount > 0) {
            return mContext.getString(R.string.safety_hub_reused_weak_account_passwords_title);
        }
        return mContext.getString(R.string.safety_hub_reused_weak_local_passwords_title);
    }

    @Override
    public String getSummary() {
        int totalWeakPasswordsCount = mAccountWeakPasswordsCount + mLocalWeakPasswordsCount;
        return mContext.getResources()
                .getQuantityString(
                        R.plurals.safety_hub_weak_passwords_summary,
                        totalWeakPasswordsCount,
                        totalWeakPasswordsCount);
    }

    @Override
    public String getPrimaryButtonText() {
        if (mAccountWeakPasswordsCount > 0 && mLocalWeakPasswordsCount > 0) {
            return mContext.getString(R.string.safety_hub_password_subpage_navigation_button);
        }
        return mContext.getString(R.string.safety_hub_passwords_navigation_button);
    }

    @Override
    public View.OnClickListener getPrimaryButtonListener() {
        if (mAccountWeakPasswordsCount > 0 && mLocalWeakPasswordsCount > 0) {
            return v -> {
                // TODO(crbug.com/407931779): Change to open the SH passwords page.
                mModuleDelegate.showLocalPasswordCheckUi(mContext);
            };
        }
        if (mAccountWeakPasswordsCount > 0) {
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
        if (mUnifiedModule && !(mAccountWeakPasswordsCount > 0 && mLocalWeakPasswordsCount > 0)) {
            return mContext.getString(R.string.safety_hub_password_subpage_navigation_button);
        }
        return null;
    }

    @Override
    public @Nullable View.OnClickListener getSecondaryButtonListener() {
        if (mUnifiedModule && !(mAccountWeakPasswordsCount > 0 && mLocalWeakPasswordsCount > 0)) {
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
