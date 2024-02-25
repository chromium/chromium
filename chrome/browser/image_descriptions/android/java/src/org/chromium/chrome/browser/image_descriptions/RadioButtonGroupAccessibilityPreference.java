// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.image_descriptions;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.RadioGroup;

import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescriptionLayout;

/**
 * A radio button group used for accessibility preference. This allows the user to toggle between
 * allowing the get image descriptions feature on mobile data, or requiring it need Wi-Fi to run.
 */
public class RadioButtonGroupAccessibilityPreference extends Preference
        implements RadioGroup.OnCheckedChangeListener {
    private RadioButtonWithDescriptionLayout mButtonGroup;
    private RadioButtonWithDescription mOnlyOnWifi;
    private RadioButtonWithDescription mUseMobileData;

    private boolean mOnlyOnWifiValue;

    public RadioButtonGroupAccessibilityPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        setLayoutResource(R.layout.radio_button_group_accessibility_preference);
    }

    public void initialize(boolean onlyOnWifi) {
        mOnlyOnWifiValue = onlyOnWifi;
    }

    public boolean getOnlyOnWifiValue() {
        return mOnlyOnWifiValue;
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        mOnlyOnWifi =
                (RadioButtonWithDescription)
                        holder.findViewById(
                                R.id.image_descriptions_settings_only_on_wifi_radio_button);
        mUseMobileData =
                (RadioButtonWithDescription)
                        holder.findViewById(
                                R.id.image_descriptions_settings_mobile_data_radio_button);

        mButtonGroup = (RadioButtonWithDescriptionLayout) mOnlyOnWifi.getParent();
        mButtonGroup.setOnCheckedChangeListener(this);

        if (mOnlyOnWifiValue) {
            mOnlyOnWifi.setChecked(true);
        } else {
            mUseMobileData.setChecked(true);
        }
    }

    @Override
    public void onCheckedChanged(RadioGroup group, int checkedId) {
        mOnlyOnWifiValue = mOnlyOnWifi.isChecked();
        callChangeListener(mOnlyOnWifiValue);
    }
}
