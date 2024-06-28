// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.RadioGroup;

import androidx.annotation.IdRes;
import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.TabArchiveSettings;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescriptionLayout;

/** A group of radio buttons to manage the archive time delta preference. */
public class TabArchiveTimeDeltaPreference extends Preference
        implements RadioGroup.OnCheckedChangeListener {
    // The time delta options.
    private static final int[] ARCHIVE_TIME_DELTA_DAYS_OPTS = new int[] {0, 7, 14, 30};
    private static final String TIME_DELTA_HISTOGRAM = "Tabs.ArchiveSettings.TimeDeltaPreference";

    private RadioButtonWithDescription[] mRadioButtons = new RadioButtonWithDescription[4];
    private TabArchiveSettings mTabArchiveSettings;

    public TabArchiveTimeDeltaPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        // Inflating from XML.
        setLayoutResource(R.xml.tab_archive_time_delta_preference);
    }

    /**
     * @param tabArchiveSettings The class to manage archive settings.
     */
    public void initialize(TabArchiveSettings tabArchiveSettings) {
        mTabArchiveSettings = tabArchiveSettings;
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        for (int i = 0; i < ARCHIVE_TIME_DELTA_DAYS_OPTS.length; i++) {
            int currentOpt = ARCHIVE_TIME_DELTA_DAYS_OPTS[i];
            RadioButtonWithDescription layout =
                    (RadioButtonWithDescription) holder.findViewById(getIdForIndex(i));
            assert layout != null;
            if (currentOpt == 0) {
                layout.setPrimaryText(
                        getContext().getString(R.string.archive_settings_time_delta_never));
            } else {
                layout.setPrimaryText(
                        getContext()
                                .getResources()
                                .getQuantityString(
                                        R.plurals.archive_settings_time_delta,
                                        currentOpt,
                                        currentOpt));
            }
            mRadioButtons[i] = layout;
        }

        if (mTabArchiveSettings.getArchiveEnabled()) {
            mRadioButtons[findIndexOfClosestPreference()].setChecked(true);
        } else {
            mRadioButtons[0].setChecked(true);
        }

        RadioButtonWithDescriptionLayout radioButtonLayout =
                (RadioButtonWithDescriptionLayout) holder.findViewById(R.id.radio_button_layout);
        radioButtonLayout.setOnCheckedChangeListener(this);
    }

    @Override
    public void onCheckedChanged(RadioGroup group, int checkedId) {
        int optIndex = group.indexOfChild(group.findViewById(checkedId));
        int opt = ARCHIVE_TIME_DELTA_DAYS_OPTS[optIndex];
        assert optIndex != -1 && optIndex < ARCHIVE_TIME_DELTA_DAYS_OPTS.length;
        if (optIndex == 0) {
            mTabArchiveSettings.setArchiveEnabled(false);
        } else {
            mTabArchiveSettings.setArchiveEnabled(true);
            mTabArchiveSettings.setArchiveTimeDeltaDays(opt);
        }
        RecordHistogram.recordCount1000Histogram(TIME_DELTA_HISTOGRAM, opt);
    }

    @VisibleForTesting
    @IdRes
    int getIdForIndex(int index) {
        switch (index) {
            case 0:
                return R.id.one;
            case 1:
                return R.id.two;
            case 2:
                return R.id.three;
            case 3:
                return R.id.four;
            default:
                assert false
                        : "Unsupported index given for preference, add a new button and return the"
                                + " id here";
                return -1;
        }
    }

    private int findIndexOfClosestPreference() {
        int currentTimeDeltaDays = mTabArchiveSettings.getArchiveTimeDeltaDays();
        int closestIndex = -1;
        int closestDiff = -1;
        for (int i = 0; i < ARCHIVE_TIME_DELTA_DAYS_OPTS.length; i++) {
            // In the case where the options change and don't match up exactly, find the closest
            // available option.
            int timeDeltaOpt = ARCHIVE_TIME_DELTA_DAYS_OPTS[i];
            if (closestDiff == -1 || Math.abs(currentTimeDeltaDays - timeDeltaOpt) < closestDiff) {
                closestIndex = i;
                closestDiff = Math.abs(currentTimeDeltaDays - timeDeltaOpt);
            }
        }

        return closestIndex;
    }

    // Testing specific methods.

    public RadioButtonWithDescription getCheckedRadioButtonForTesting() {
        for (RadioButtonWithDescription button : mRadioButtons) {
            if (button.isChecked()) return button;
        }

        return null;
    }

    public RadioButtonWithDescription getRadioButtonForTesting(int index) {
        return mRadioButtons[index];
    }
}
