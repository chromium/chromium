// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Based on frameworks/base/core/java/android/preference/SeekBarPreference.java,
// extended to support floating-point min/max/step and a summary label.

package org.chromium.chrome.browser.preferences;

import android.content.Context;
import android.support.v7.preference.Preference;
import android.support.v7.preference.PreferenceViewHolder;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.widget.SeekBar;
import android.widget.SeekBar.OnSeekBarChangeListener;
import android.widget.TextView;

import org.chromium.chrome.R;

/**
 * A preference that allows the user to set a value by sliding a seek bar.
 *
 * When this preference is used, a layout must be provided that contains a SeekBar with ID "seekbar"
 * and a TextView with ID "seekbar_amount".
 */
public class SeekBarPreference extends Preference implements OnSeekBarChangeListener {

    private float mMin, mMax, mStep;
    private float mValue;
    private boolean mTrackingTouch;
    CharSequence mSummary;
    private TextView mSummaryView;

    /**
     * Constructor for inflating from XML.
     */
    public SeekBarPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        mMin = 0.5f;
        mMax = 2.0f;
        mStep = 0.05f;
        mValue = mMin;
    }

    /**
     * Sets the progress value for the seekbar. The value will be adjusted, if needed, to ensure
     * it's within the valid range.
     */
    public void setValue(float value) {
        setValue(value, true);
    }

    /**
     * Returns whether the user is currently dragging the seek bar.
     */
    public boolean isTrackingTouch() {
        return mTrackingTouch;
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        SeekBar seekBar = (SeekBar) holder.findViewById(R.id.seekbar);
        seekBar.setOnSeekBarChangeListener(this);
        seekBar.setMax(prefValueToSeekBarProgress(mMax));
        seekBar.setProgress(prefValueToSeekBarProgress(mValue));
        seekBar.setEnabled(isEnabled());
        mSummaryView = (TextView) holder.findViewById(R.id.seekbar_amount);
        mSummaryView.setText(mSummary);
    }

    /**
     * Sets the summary for this Preference with a CharSequence. Unlike the superclass
     * implementation, this does not call notifyChanged() as that would cancel the
     * current slider drag.
     *
     * @param summary The summary for the preference.
     */
    @Override
    public void setSummary(CharSequence summary) {
        if (TextUtils.equals(summary, mSummary)) return;

        mSummary = summary;
        if (mSummaryView != null) mSummaryView.setText(summary);
    }

    @Override
    public CharSequence getSummary() {
        return mSummary;
    }

    private float seekBarProgressToPrefValue(int seekBarProgress) {
        // SeekBar only supports integer steps, and always starts from 0.
        // So must convert floating point pref values to/from integers,
        // appropriately scaled for the SeekBar.
        return mMin + seekBarProgress * mStep;
    }

    private int prefValueToSeekBarProgress(float prefValue) {
        return Math.round((prefValue - mMin) / mStep);
    }

    private void setValue(float value, boolean notifyChanged) {
        value = Math.min(mMax, Math.max(mMin, value));
        if (value != mValue) {
            mValue = value;
            if (notifyChanged) notifyChanged();
        }
    }

    /**
     * Persist the seekBar's progress value if callChangeListener
     * returns true, otherwise set the seekBar's progress to the stored value
     */
    private void syncProgress(SeekBar seekBar) {
        float value = seekBarProgressToPrefValue(seekBar.getProgress());
        if (value != mValue) {
            if (callChangeListener(value)) {
                setValue(value, false);
            } else {
                seekBar.setProgress(prefValueToSeekBarProgress(mValue));
            }
        }
    }

    @Override
    public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
        if (fromUser) syncProgress(seekBar);
    }

    @Override
    public void onStartTrackingTouch(SeekBar seekBar) {
        mTrackingTouch = true;
    }

    @Override
    public void onStopTrackingTouch(SeekBar seekBar) {
        mTrackingTouch = false;
    }
}
