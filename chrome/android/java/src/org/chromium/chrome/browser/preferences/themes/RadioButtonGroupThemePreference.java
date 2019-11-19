// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.themes;

import android.content.Context;
import android.support.v7.preference.Preference;
import android.support.v7.preference.PreferenceViewHolder;
import android.util.AttributeSet;
import android.view.View;
import android.widget.CheckBox;
import android.widget.LinearLayout;
import android.widget.RadioGroup;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.BuildInfo;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.night_mode.NightModeMetrics;
import org.chromium.chrome.browser.preferences.themes.ThemePreferences.ThemeSetting;
import org.chromium.chrome.browser.ui.widget.RadioButtonWithDescription;
import org.chromium.chrome.browser.ui.widget.RadioButtonWithDescriptionLayout;

import java.util.ArrayList;
import java.util.Collections;

/**
 * A radio button group Preference used for Themes. Currently, it has 3 options: System default,
 * Light, and Dark. When an additional flag, DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING, is active
 * there is an option added underneath the currently selected preference to allow website contents
 * to be darkened (active for System default and Dark).
 */
public class RadioButtonGroupThemePreference
        extends Preference implements RadioGroup.OnCheckedChangeListener {
    private @ThemeSetting int mSetting;
    private RadioButtonWithDescription mSettingRadioButton;
    private RadioButtonWithDescriptionLayout mGroup;
    private ArrayList<RadioButtonWithDescription> mButtons;

    // Additional view that darkens website contents.
    private LinearLayout mCheckboxContainer;
    private boolean mDarkenWebsitesEnabled;
    private CheckBox mCheckBox;

    public RadioButtonGroupThemePreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        // Inflating from XML.
        setLayoutResource(R.layout.radio_button_group_theme_preference);

        // Initialize entries with null objects so that calling ArrayList#set() would not throw
        // java.lang.IndexOutOfBoundsException.
        mButtons = new ArrayList<>(Collections.nCopies(ThemeSetting.NUM_ENTRIES, null));
    }

    /**
     * @param setting The initial setting for this Preference
     */
    public void initialize(@ThemeSetting int setting, boolean darkenWebsitesEnabled) {
        mSetting = setting;
        mDarkenWebsitesEnabled = darkenWebsitesEnabled;
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        mCheckboxContainer = (LinearLayout) holder.findViewById(R.id.checkbox_container);
        mCheckBox = (CheckBox) holder.findViewById(R.id.darken_websites);

        mGroup = (RadioButtonWithDescriptionLayout) holder.findViewById(R.id.radio_button_layout);
        mGroup.setOnCheckedChangeListener(this);

        mCheckboxContainer.setOnClickListener(x -> {
            mCheckBox.setChecked(!mCheckBox.isChecked());
            callChangeListener(mSetting);
        });

        mCheckBox.setChecked(mDarkenWebsitesEnabled);

        assert ThemeSetting.NUM_ENTRIES == 3;
        mButtons.set(ThemeSetting.SYSTEM_DEFAULT,
                (RadioButtonWithDescription) holder.findViewById(R.id.system_default));
        if (BuildInfo.isAtLeastQ()) {
            mButtons.get(ThemeSetting.SYSTEM_DEFAULT)
                    .setDescriptionText(
                            getContext().getString(R.string.themes_system_default_summary_api_29));
        }
        mButtons.set(
                ThemeSetting.LIGHT, (RadioButtonWithDescription) holder.findViewById(R.id.light));
        mButtons.set(
                ThemeSetting.DARK, (RadioButtonWithDescription) holder.findViewById(R.id.dark));

        mSettingRadioButton = mButtons.get(mSetting);
        mSettingRadioButton.setChecked(true);
        positionCheckbox();
    }

    /**
     * Remove and insert the checkbox to the view, based on the current theme preference.
     */
    private void positionCheckbox() {
        if (ChromeFeatureList.isEnabled(
                    ChromeFeatureList.DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING)) {
            if (mSetting == ThemeSetting.SYSTEM_DEFAULT || mSetting == ThemeSetting.DARK) {
                mGroup.attachAccessoryView(mCheckboxContainer, mSettingRadioButton);
                mCheckboxContainer.setVisibility(View.VISIBLE);
            } else {
                mCheckboxContainer.setVisibility(View.GONE);
            }
        }
    }

    @Override
    public void onCheckedChanged(RadioGroup group, int checkedId) {
        for (int i = 0; i < ThemeSetting.NUM_ENTRIES; i++) {
            if (mButtons.get(i).isChecked()) {
                mSetting = i;
                mSettingRadioButton = mButtons.get(i);
                break;
            }
        }
        assert mSetting >= 0 && mSetting < ThemeSetting.NUM_ENTRIES : "No matching setting found.";

        positionCheckbox();
        callChangeListener(mSetting);
        NightModeMetrics.recordThemePreferencesChanged(mSetting);
    }

    public boolean isDarkenWebsitesEnabled() {
        return mCheckBox.isChecked();
    }

    @VisibleForTesting
    public int getSetting() {
        return mSetting;
    }

    @VisibleForTesting
    ArrayList getButtonsForTesting() {
        return mButtons;
    }
}
