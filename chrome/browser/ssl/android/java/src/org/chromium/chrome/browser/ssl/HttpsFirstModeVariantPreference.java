// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ssl;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.RadioGroup;

import androidx.preference.PreferenceViewHolder;

import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.settings.ContainedRadioButtonGroupPreference;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescriptionLayout;

/**
 * HttpsFirstModeVariantPreference is the user interface that is shown when HTTPS-First Mode is
 * enabled. When HTTPS-First Mode is off, the HttpsFirstModeVariantPreference is disabled. (This is
 * a boolean choice but displayed as a radio button group.)
 */
@NullMarked
class HttpsFirstModeVariantPreference extends ContainedRadioButtonGroupPreference
        implements RadioGroup.OnCheckedChangeListener {
    // UI elements. These fields are assigned only once, in onBindViewHolder.
    private RadioButtonWithDescriptionLayout mGroupLayout;
    private RadioButtonWithDescription mBalancedButton;
    private RadioButtonWithDescription mStrictButton;

    // Current setting.
    private @HttpsFirstModeSetting int mHttpsFirstModeSetting;

    public HttpsFirstModeVariantPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        setLayoutResource(R.layout.https_first_mode_variant_preference);
    }

    public void init(@HttpsFirstModeSetting int httpsFirstModeSetting) {
        mHttpsFirstModeSetting = httpsFirstModeSetting;
    }

    @Override
    public void onCheckedChanged(RadioGroup group, int checkedId) {
        if (checkedId == mBalancedButton.getId()) {
            mHttpsFirstModeSetting = HttpsFirstModeSetting.ENABLED_BALANCED;
        } else if (checkedId == mStrictButton.getId()) {
            mHttpsFirstModeSetting = HttpsFirstModeSetting.ENABLED_FULL;
        }
        callChangeListener(mHttpsFirstModeSetting);
    }

    @Initializer
    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        mGroupLayout = (RadioButtonWithDescriptionLayout) holder.findViewById(R.id.mode_group);
        mGroupLayout.setOnCheckedChangeListener(this);

        mBalancedButton = (RadioButtonWithDescription) holder.findViewById(R.id.balanced);
        mStrictButton = (RadioButtonWithDescription) holder.findViewById(R.id.strict);

        setCheckedState(mHttpsFirstModeSetting);
    }

    public void setCheckedState(@HttpsFirstModeSetting int checkedState) {
        if (mGroupLayout == null) {
            // Not yet bound to view holder.
            return;
        }

        mHttpsFirstModeSetting = checkedState;
        mStrictButton.setChecked(checkedState == HttpsFirstModeSetting.ENABLED_FULL);
        mBalancedButton.setChecked(
                checkedState == HttpsFirstModeSetting.ENABLED_BALANCED
                        || checkedState == HttpsFirstModeSetting.DISABLED);
    }

    public RadioButtonWithDescription getStrictModeButtonForTesting() {
        return mStrictButton;
    }

    public RadioButtonWithDescription getBalancedModeButtonForTesting() {
        return mBalancedButton;
    }
}
