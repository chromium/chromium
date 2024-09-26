// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.options;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.RadioGroup;

import androidx.annotation.IdRes;
import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.chrome.browser.autofill.R;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescriptionLayout;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** A radio button group toggling the opt-in status for Third Party support. */
public final class RadioButtonGroupThirdPartyPreference extends Preference {
    /** Enums that represent the status of radio buttons inside this Preference. */
    @IntDef({ThirdPartyOption.DEFAULT, ThirdPartyOption.USE_OTHER_PROVIDER})
    @Retention(RetentionPolicy.SOURCE)
    @interface ThirdPartyOption {
        int DEFAULT = 0;
        int USE_OTHER_PROVIDER = 1;

        int NUM_ENTRIES = 2;
    }

    private @Nullable RadioButtonWithDescription mDefaultOption;
    private @Nullable RadioButtonWithDescription mOptInOption;
    private @ThirdPartyOption int mSelectedOption = getPersistedInt(ThirdPartyOption.DEFAULT);

    public RadioButtonGroupThirdPartyPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        setLayoutResource(R.layout.radio_button_group_third_party_preference);
    }

    /**
     * Sets the selected option which is reflected in checking only the corresponding radio button.
     * If the buttons aren't available yet, the selected option will have an effect during binding.
     * This method invokes change listeners if the selected option changes and it is a noop if the
     * option is unchanged.
     */
    void setSelectedOption(@ThirdPartyOption int selectedOption) {
        if (mDefaultOption != null) {
            mDefaultOption.setChecked(selectedOption == ThirdPartyOption.DEFAULT);
        }
        if (mOptInOption != null) {
            mOptInOption.setChecked(selectedOption == ThirdPartyOption.USE_OTHER_PROVIDER);
        }
        if (mSelectedOption != selectedOption) {
            mSelectedOption = selectedOption;
            callChangeListener(mSelectedOption);
            persistInt(mSelectedOption);
        }
    }

    /**
     * Returns the option representing the selected radio button.
     *
     * @return A {@link ThirdPartyOption}
     */
    @ThirdPartyOption
    int getSelectedThirdPartyOption() {
        return mSelectedOption;
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        mDefaultOption =
                (RadioButtonWithDescription)
                        holder.findViewById(R.id.autofill_third_party_filling_default);
        mOptInOption =
                (RadioButtonWithDescription)
                        holder.findViewById(R.id.autofill_third_party_filling_opt_in);
        RadioButtonWithDescriptionLayout group =
                (RadioButtonWithDescriptionLayout)
                        holder.findViewById(R.id.autofill_third_party_radio_group);

        group.setOnCheckedChangeListener(this::onCheckedChanged);
        setSelectedOption(mSelectedOption); // Update UI according to internal state.
    }

    /**
     * Returns the selected option
     *
     * @return A {@link ThirdPartyOption}.
     */
    @VisibleForTesting
    @ThirdPartyOption
    int getSelectedOption() {
        return mSelectedOption;
    }

    /**
     * Returns the radio button for the default option.
     *
     * @return A {@link RadioButtonWithDescription} after layout inflation.
     */
    @VisibleForTesting
    @Nullable
    RadioButtonWithDescription getDefaultButton() {
        return mDefaultOption;
    }

    /**
     * Returns the radio button for the option to opt into another provider.
     *
     * @return A {@link RadioButtonWithDescription} after layout inflation.
     */
    @VisibleForTesting
    @Nullable
    RadioButtonWithDescription getOptInButton() {
        return mOptInOption;
    }

    private void onCheckedChanged(RadioGroup group, @IdRes int checkedId) {
        setSelectedOption(getOptionForId(checkedId));
    }

    private static @ThirdPartyOption int getOptionForId(@IdRes int id) {
        if (id == R.id.autofill_third_party_filling_default) {
            return ThirdPartyOption.DEFAULT;
        }
        if (id == R.id.autofill_third_party_filling_opt_in) {
            return ThirdPartyOption.USE_OTHER_PROVIDER;
        }
        assert false : "No ThirdPartyOption maps to id: " + id;
        return ThirdPartyOption.DEFAULT;
    }
}
