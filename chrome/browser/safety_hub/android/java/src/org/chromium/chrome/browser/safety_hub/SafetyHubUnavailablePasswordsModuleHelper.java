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

/**
 * Helper for the {@link SafetyHubPasswordsModule} for when passwords counts are unavailable module.
 */
@NullMarked
public class SafetyHubUnavailablePasswordsModuleHelper implements SafetyHubModuleHelper {
    private final Context mContext;
    private final SafetyHubModuleDelegate mModuleDelegate;
    private final boolean mUnavailableAccountPasswords;
    private final boolean mUnavailableLocalPasswords;

    SafetyHubUnavailablePasswordsModuleHelper(
            Context context,
            SafetyHubModuleDelegate moduleDelegate,
            boolean unavailableAccountPasswords,
            boolean unavailableLocalPasswords) {
        mContext = context;
        mModuleDelegate = moduleDelegate;
        mUnavailableAccountPasswords = unavailableAccountPasswords;
        mUnavailableLocalPasswords = unavailableLocalPasswords;
    }

    @Override
    public String getTitle() {
        if (mUnavailableAccountPasswords && mUnavailableLocalPasswords) {
            return mContext.getString(R.string.safety_hub_password_check_unavailable_title);
        }
        if (mUnavailableAccountPasswords) {
            return mContext.getString(R.string.safety_hub_account_password_check_unavailable_title);
        }
        return mContext.getString(R.string.safety_hub_local_password_check_unavailable_title);
    }

    @Override
    public String getSummary() {
        return mContext.getString(R.string.safety_hub_unavailable_summary);
    }

    @Override
    public @Nullable String getPrimaryButtonText() {
        return null;
    }

    @Override
    public @Nullable View.OnClickListener getPrimaryButtonListener() {
        return null;
    }

    @Override
    public String getSecondaryButtonText() {
        if (mUnavailableAccountPasswords && mUnavailableLocalPasswords) {
            return mContext.getString(R.string.safety_hub_password_subpage_navigation_button);
        }
        return mContext.getString(R.string.safety_hub_passwords_navigation_button);
    }

    @Override
    public View.OnClickListener getSecondaryButtonListener() {
        if (mUnavailableAccountPasswords && mUnavailableLocalPasswords) {
            return v -> {
                // TODO(crbug.com/407931779): Change to open the SH passwords page.
                mModuleDelegate.showLocalPasswordCheckUi(mContext);
            };
        }
        if (mUnavailableAccountPasswords) {
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
    public @ModuleState int getModuleState() {
        return ModuleState.UNAVAILABLE;
    }
}
