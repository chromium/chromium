// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.startup;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.RadioGroup;

import androidx.preference.PreferenceViewHolder;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.multiwindow.SessionStartupPref;
import org.chromium.components.browser_ui.settings.ContainedRadioButtonGroupPreference;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescriptionLayout;

/**
 * A radio button group preference for selecting the session startup behavior ({@link
 * SessionStartupPref#NEW_TAB}, {@link SessionStartupPref#LAST}, or {@link
 * SessionStartupPref#URLS}).
 */
@NullMarked
public class RadioButtonGroupOnStartupPreference extends ContainedRadioButtonGroupPreference
        implements RadioGroup.OnCheckedChangeListener {
    private @Nullable RadioButtonWithDescription mNewTabPref;
    private @Nullable RadioButtonWithDescription mLastPref;
    private @Nullable RadioButtonWithDescription mUrlsPref;
    private @SessionStartupPref int mStartupType = SessionStartupPref.LAST;
    private @Nullable OnStartupTypeChangeListener mOnStartupTypeChangeListener;

    /** Constructs a new {@link RadioButtonGroupOnStartupPreference}. */
    public RadioButtonGroupOnStartupPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        setLayoutResource(R.layout.radio_button_group_on_startup_preference);
    }

    // ContainedRadioButtonGroupPreference implementation.
    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        mNewTabPref = (RadioButtonWithDescription) holder.findViewById(R.id.pref_new_tab);
        mLastPref = (RadioButtonWithDescription) holder.findViewById(R.id.pref_last);
        mUrlsPref = (RadioButtonWithDescription) holder.findViewById(R.id.pref_urls);

        RadioButtonWithDescriptionLayout radioGroup =
                (RadioButtonWithDescriptionLayout) holder.findViewById(R.id.on_startup_radio_group);
        radioGroup.setOnCheckedChangeListener(this);

        updateCheckedState();
    }

    // RadioGroup.OnCheckedChangeListener implementation.
    @Override
    public void onCheckedChanged(RadioGroup group, int checkedId) {
        if (checkedId == R.id.pref_new_tab) {
            mStartupType = SessionStartupPref.NEW_TAB;
        } else if (checkedId == R.id.pref_last) {
            mStartupType = SessionStartupPref.LAST;
        } else if (checkedId == R.id.pref_urls) {
            mStartupType = SessionStartupPref.URLS;
        }
        if (mOnStartupTypeChangeListener != null) {
            mOnStartupTypeChangeListener.onStartupTypeChanged(mStartupType);
        }
    }

    /* package */ void initialize(
            @SessionStartupPref int startupType, OnStartupTypeChangeListener listener) {
        mStartupType = startupType;
        mOnStartupTypeChangeListener = listener;
        updateCheckedState();
    }

    private void updateCheckedState() {
        if (mNewTabPref == null || mLastPref == null || mUrlsPref == null) {
            return;
        }
        mNewTabPref.setChecked(mStartupType == SessionStartupPref.NEW_TAB);
        mLastPref.setChecked(mStartupType == SessionStartupPref.LAST);
        mUrlsPref.setChecked(mStartupType == SessionStartupPref.URLS);
    }

    /* package */ @Nullable RadioButtonWithDescription getNewTabPrefForTesting() {
        return mNewTabPref;
    }

    /* package */ @Nullable RadioButtonWithDescription getLastPrefForTesting() {
        return mLastPref;
    }

    /* package */ @Nullable RadioButtonWithDescription getUrlsPrefForTesting() {
        return mUrlsPref;
    }

    /* package */ interface OnStartupTypeChangeListener {
        void onStartupTypeChanged(@SessionStartupPref int startupType);
    }
}
