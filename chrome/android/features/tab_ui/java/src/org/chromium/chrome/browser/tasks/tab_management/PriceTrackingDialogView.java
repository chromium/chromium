// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.LinearLayout;

import androidx.appcompat.widget.SwitchCompat;

import org.chromium.chrome.tab_ui.R;

/**
 * The view for PriceTrackingSettings dialog related UIs.
 */
public class PriceTrackingDialogView extends LinearLayout {
    private SwitchCompat mTrackPricesSwitch;
    private SwitchCompat mPriceAlertsSwitch;

    public PriceTrackingDialogView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTrackPricesSwitch = (SwitchCompat) findViewById(R.id.track_prices_switch);
        mPriceAlertsSwitch = (SwitchCompat) findViewById(R.id.price_alerts_switch);
    }

    /**
     * Update the checked/unchecked status of two switches. This is called every time before the
     * dialog shows.
     */
    void updateSwitch() {
        mTrackPricesSwitch.setChecked(PriceTrackingUtilities.isTrackPricesOnTabsEnabled());
        mPriceAlertsSwitch.setChecked(PriceTrackingUtilities.isPriceDropAlertsEnabled());
    }

    /**
     * Set the OnCheckedChangeListener of two switches.
     */
    void setupOnCheckedChangeListener(OnCheckedChangeListener onCheckedChangeListener) {
        mTrackPricesSwitch.setOnCheckedChangeListener(onCheckedChangeListener);
        mPriceAlertsSwitch.setOnCheckedChangeListener(onCheckedChangeListener);
    }
}
