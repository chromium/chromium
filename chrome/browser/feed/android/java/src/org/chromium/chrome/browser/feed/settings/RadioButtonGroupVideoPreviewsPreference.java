// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.settings;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.RadioGroup;

import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.feed.R;
import org.chromium.chrome.browser.feed.VideoPreviewsType;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescriptionLayout;

import java.util.ArrayList;
import java.util.Collections;

/**
 * A radio button group used for video previews preference. This allows the user to choose among:
 * never showing video previews, showing on Wi-Fi only, and showing on both Wi-Fi and mobile data.
 */
public class RadioButtonGroupVideoPreviewsPreference
        extends Preference implements RadioGroup.OnCheckedChangeListener {
    private @VideoPreviewsType int mSetting;
    private ArrayList<RadioButtonWithDescription> mButtons;

    public RadioButtonGroupVideoPreviewsPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        setLayoutResource(R.layout.radio_button_group_video_previews_preference);

        // Initialize entries with null objects so that calling ArrayList#set() would not throw
        // java.lang.IndexOutOfBoundsException.
        mButtons = new ArrayList<>(Collections.nCopies(VideoPreviewsType.NUM_ENTRIES, null));
    }

    public void initialize(@VideoPreviewsType int setting) {
        mSetting = setting;
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        assert VideoPreviewsType.NUM_ENTRIES == 3;
        mButtons.set(VideoPreviewsType.NEVER,
                (RadioButtonWithDescription) holder.findViewById(
                        R.id.video_previews_option_never_radio_button));
        mButtons.set(VideoPreviewsType.WIFI,
                (RadioButtonWithDescription) holder.findViewById(
                        R.id.video_previews_option_wifi_radio_button));
        mButtons.set(VideoPreviewsType.WIFI_AND_MOBILE_DATA,
                (RadioButtonWithDescription) holder.findViewById(
                        R.id.video_previews_option_wifi_and_mobile_data_radio_button));

        RadioButtonWithDescription settingRadioButton = mButtons.get(mSetting);
        settingRadioButton.setChecked(true);

        RadioButtonWithDescriptionLayout buttonGroup =
                (RadioButtonWithDescriptionLayout) settingRadioButton.getParent();
        buttonGroup.setOnCheckedChangeListener(this);
    }

    @Override
    public void onCheckedChanged(RadioGroup group, int checkedId) {
        for (int i = 0; i < VideoPreviewsType.NUM_ENTRIES; i++) {
            if (mButtons.get(i).isChecked()) {
                mSetting = i;
                break;
            }
        }
        assert mSetting >= 0
                && mSetting < VideoPreviewsType.NUM_ENTRIES : "No matching setting found.";

        RecordHistogram.recordEnumeratedHistogram(
                "FeedVideoPreviewsPreferenceUserActions", mSetting, VideoPreviewsType.NUM_ENTRIES);

        callChangeListener(mSetting);
    }
}
