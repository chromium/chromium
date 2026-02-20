// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.RadioGroup;

import androidx.annotation.IntDef;
import androidx.preference.PreferenceViewHolder;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.components.browser_ui.settings.ContainedRadioButtonGroupPreference;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescriptionLayout;
import org.chromium.components.user_prefs.UserPrefs;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@NullMarked
public class KeyboardShortcutRadioButtonGroupPreference extends ContainedRadioButtonGroupPreference
        implements RadioGroup.OnCheckedChangeListener {

    private static final String KEYWORD_SPACE_TRIGGERING_ENABLED =
            "omnibox.keyword_space_triggering_enabled";

    @IntDef({KeyboardShortcut.SPACE_OR_TAB, KeyboardShortcut.TAB})
    @Retention(RetentionPolicy.SOURCE)
    private @interface KeyboardShortcut {
        int SPACE_OR_TAB = 0;
        int TAB = 1;
    }

    private @Nullable Profile mProfile;
    private @KeyboardShortcut int mSelectedOption;
    private @Nullable RadioButtonWithDescription mSpaceOrTabButton;
    private @Nullable RadioButtonWithDescription mTabButton;
    private @Nullable RadioButtonWithDescriptionLayout mGroup;

    public KeyboardShortcutRadioButtonGroupPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        setLayoutResource(R.layout.keyboard_shortcut_radio_group);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        mSpaceOrTabButton =
                (RadioButtonWithDescription)
                        holder.findViewById(R.id.keyboard_shortcut_space_or_tab);
        mTabButton = (RadioButtonWithDescription) holder.findViewById(R.id.keyboard_shortcut_tab);
        mGroup =
                (RadioButtonWithDescriptionLayout)
                        holder.findViewById(R.id.keyboard_shortcut_radio_group);
        mGroup.setOnCheckedChangeListener(this);

        if (mProfile != null) {
            boolean enabled = UserPrefs.get(mProfile).getBoolean(KEYWORD_SPACE_TRIGGERING_ENABLED);
            mSelectedOption = enabled ? KeyboardShortcut.SPACE_OR_TAB : KeyboardShortcut.TAB;
        }
        updateCheckedState(mSelectedOption);
    }

    @Override
    public void onCheckedChanged(RadioGroup group, int checkedId) {
        @KeyboardShortcut
        int newOption =
                (R.id.keyboard_shortcut_space_or_tab == checkedId)
                        ? KeyboardShortcut.SPACE_OR_TAB
                        : KeyboardShortcut.TAB;

        if (mSelectedOption != newOption) {
            mSelectedOption = newOption;
            if (mProfile != null) {
                boolean enabled = (newOption == KeyboardShortcut.SPACE_OR_TAB);
                UserPrefs.get(mProfile).setBoolean(KEYWORD_SPACE_TRIGGERING_ENABLED, enabled);
            }
            callChangeListener(newOption);
        }
    }

    public void setProfile(Profile profile) {
        mProfile = profile;
    }

    private void updateCheckedState(@KeyboardShortcut int option) {
        if (mSpaceOrTabButton != null && mTabButton != null) {
            if (option == KeyboardShortcut.SPACE_OR_TAB) {
                mSpaceOrTabButton.setChecked(true);
            } else {
                mTabButton.setChecked(true);
            }
        }
    }
}
