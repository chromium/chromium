// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.content.Context;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.preference.PreferenceViewHolder;

import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.ui.widget.ButtonCompat;

class SafetyHubPermissionsPreference extends ChromeBasePreference implements View.OnClickListener {
    private final @NonNull PermissionsData mPermissionsData;

    SafetyHubPermissionsPreference(Context context, @NonNull PermissionsData permissionsData) {
        super(context);

        mPermissionsData = permissionsData;
        setTitle(mPermissionsData.getOrigin());
        // TODO(crbug.com/324562205): Display correct substring based on revoked
        // permissions.
        setSelectable(false);
        setWidgetLayoutResource(R.layout.safety_hub_button_widget);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        ButtonCompat button = (ButtonCompat) holder.findViewById(R.id.button);
        button.setText(R.string.undo);
        button.setOnClickListener(this);
    }

    @Override
    public void onClick(View v) {
        if (getOnPreferenceClickListener() != null) {
            getOnPreferenceClickListener().onPreferenceClick(this);
        }
    }

    @NonNull
    PermissionsData getPermissionsData() {
        return mPermissionsData;
    }
}
