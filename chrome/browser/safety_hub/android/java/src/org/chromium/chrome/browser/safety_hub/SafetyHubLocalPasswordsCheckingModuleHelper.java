// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.content.Context;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.safety_hub.SafetyHubModuleMediator.ModuleState;

/** Helper for the {@link SafetyHubLocalPasswordsModule} for when the checkup is being performed. */
@NullMarked
public class SafetyHubLocalPasswordsCheckingModuleHelper implements SafetyHubModuleHelper {
    private final Context mContext;

    SafetyHubLocalPasswordsCheckingModuleHelper(Context context) {
        mContext = context;
    }

    @Override
    public String getTitle() {
        return mContext.getString(R.string.safety_hub_local_passwords_checking_title);
    }

    @Override
    public @Nullable String getSummary() {
        return null;
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
    public @Nullable String getSecondaryButtonText() {
        return null;
    }

    @Override
    public View.@Nullable OnClickListener getSecondaryButtonListener() {
        return null;
    }

    @Override
    public @ModuleState int getModuleState() {
        return ModuleState.LOADING;
    }
}
