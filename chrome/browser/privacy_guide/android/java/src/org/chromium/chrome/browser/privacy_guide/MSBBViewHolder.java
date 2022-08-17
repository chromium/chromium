// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import android.view.View;

import androidx.appcompat.widget.SwitchCompat;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;

/**
 * Controls the behavior of the MSBB privacy guide page.
 */
public class MSBBViewHolder extends RecyclerView.ViewHolder {
    private final View mView;

    public MSBBViewHolder(View view) {
        super(view);
        mView = view;

        SwitchCompat msbbSwitch = mView.findViewById(R.id.msbb_switch);
        msbbSwitch.setChecked(UnifiedConsentServiceBridge.isUrlKeyedAnonymizedDataCollectionEnabled(
                Profile.getLastUsedRegularProfile()));

        msbbSwitch.setOnCheckedChangeListener(
                (button, isChecked)
                        -> UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(
                                Profile.getLastUsedRegularProfile(), isChecked));
    }
}
