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

/** Helper for the {@link SafetyHubAccountPasswordsModule} for the has no passwords state. */
@NullMarked
public class SafetyHubNoSavedPasswordsModuleHelper implements SafetyHubModuleHelper {
    private final Context mContext;
    private final SafetyHubModuleDelegate mModuleDelegate;
    private final boolean mNoAccountPasswords;
    private final boolean mNoLocalPasswords;

    SafetyHubNoSavedPasswordsModuleHelper(
            Context context,
            SafetyHubModuleDelegate moduleDelegate,
            boolean noAccountPasswords,
            boolean noLocalPasswords) {
        mContext = context;
        mModuleDelegate = moduleDelegate;
        mNoAccountPasswords = noAccountPasswords;
        mNoLocalPasswords = noLocalPasswords;

        assert noAccountPasswords || noLocalPasswords
                : "Some storage should be empty in NoSavedPasswordsModuleHelper";
    }

    @Override
    public String getTitle() {
        if (mNoAccountPasswords && mNoLocalPasswords) {
            return mContext.getString(R.string.safety_hub_no_passwords_title);
        }
        if (mNoAccountPasswords) {
            return mContext.getString(R.string.safety_hub_no_account_passwords_title);
        }
        return mContext.getString(R.string.safety_hub_no_local_passwords_title);
    }

    @Override
    public String getSummary() {
        return mContext.getString(R.string.safety_hub_no_passwords_summary);
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
        if (mNoAccountPasswords && mNoLocalPasswords) {
            return mContext.getString(R.string.safety_hub_password_subpage_navigation_button);
        }
        return mContext.getString(R.string.safety_hub_passwords_navigation_button);
    }

    @Override
    public View.OnClickListener getSecondaryButtonListener() {
        if (mNoAccountPasswords && mNoLocalPasswords) {
            return v -> {
                // TODO(crbug.com/407931779): Change to open the SH passwords page.
                mModuleDelegate.showLocalPasswordCheckUi(mContext);
            };
        }
        if (mNoAccountPasswords) {
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
        return ModuleState.INFO;
    }
}
