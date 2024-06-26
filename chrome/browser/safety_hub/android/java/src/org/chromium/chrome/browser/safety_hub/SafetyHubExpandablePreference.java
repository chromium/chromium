// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.content.Context;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.preference.PreferenceViewHolder;

import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.ui.widget.ButtonCompat;

public class SafetyHubExpandablePreference extends ChromeBasePreference {
    private String mPrimaryButtonText;
    private String mSecondaryButtonText;

    public SafetyHubExpandablePreference(Context context, AttributeSet attrs) {
        super(context, attrs);

        setLayoutResource(R.layout.safety_hub_expandable_preference);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        ButtonCompat primaryButton = (ButtonCompat) holder.findViewById(R.id.primary_button);
        assert primaryButton != null;

        if (!TextUtils.isEmpty(mPrimaryButtonText)) {
            primaryButton.setText(mPrimaryButtonText);
            primaryButton.setVisibility(View.VISIBLE);
        } else {
            primaryButton.setVisibility(View.GONE);
        }

        ButtonCompat secondaryButton = (ButtonCompat) holder.findViewById(R.id.secondary_button);
        assert secondaryButton != null;
        if (!TextUtils.isEmpty(mSecondaryButtonText)) {
            secondaryButton.setText(mSecondaryButtonText);
            secondaryButton.setVisibility(View.VISIBLE);
        } else {
            secondaryButton.setVisibility(View.GONE);
        }
    }

    void setPrimaryButtonText(@Nullable String buttonText) {
        if (!TextUtils.equals(mPrimaryButtonText, buttonText)) {
            mPrimaryButtonText = buttonText;
            this.notifyChanged();
        }
    }

    void setSecondaryButtonText(@Nullable String buttonText) {
        if (!TextUtils.equals(mSecondaryButtonText, buttonText)) {
            mSecondaryButtonText = buttonText;
            this.notifyChanged();
        }
    }

    @Nullable
    String getPrimaryButtonText() {
        return mPrimaryButtonText;
    }

    @Nullable
    String getSecondaryButtonText() {
        return mSecondaryButtonText;
    }
}
