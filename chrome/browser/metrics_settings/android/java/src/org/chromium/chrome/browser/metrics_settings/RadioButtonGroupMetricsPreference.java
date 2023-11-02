// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics_settings;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.RadioGroup;

import androidx.preference.Preference;

/**
 * A radio button group used for Metrics settings. Currently, it has 3 options: Extended metrics,
 * Basic metrics, and No metrics.
 */
public class RadioButtonGroupMetricsPreference
        extends Preference implements RadioGroup.OnCheckedChangeListener {
    public RadioButtonGroupMetricsPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        setLayoutResource(R.layout.radio_button_group_metrics_preference);
    }

    @Override
    public void onCheckedChanged(RadioGroup group, int checkedId) {}
}
