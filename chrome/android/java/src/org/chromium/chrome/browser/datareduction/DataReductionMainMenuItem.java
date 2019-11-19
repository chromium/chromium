// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.datareduction;

import android.content.Context;
import android.os.Bundle;
import android.text.format.DateUtils;
import android.text.format.Formatter;
import android.util.AttributeSet;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.preferences.datareduction.DataReductionPreferenceFragment;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.third_party.android.datausagechart.ChartDataUsageView;

/**
 * Specific {@link FrameLayout} that displays the data savings of Data Saver in the main menu.
 */
public class DataReductionMainMenuItem extends FrameLayout implements View.OnClickListener {
    /**
     * Constructs a new {@link DataReductionMainMenuItem} with the appropriate context.
     */
    public DataReductionMainMenuItem(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        TextView itemText = (TextView) findViewById(R.id.menu_item_text);
        TextView itemSummary = (TextView) findViewById(R.id.menu_item_summary);
        ImageView icon = (ImageView) findViewById(R.id.icon);
        icon.setContentDescription(getContext().getString(R.string.data_reduction_title_lite_mode));

        if (DataReductionProxySettings.getInstance().isDataReductionProxyEnabled()) {
            DataReductionProxyUma.dataReductionProxyUIAction(
                    DataReductionProxyUma.ACTION_MAIN_MENU_DISPLAYED_ON);

            String dataSaved = Formatter.formatShortFileSize(getContext(),
                    DataReductionProxySettings.getInstance()
                            .getContentLengthSavedInHistorySummary());

            long chartStartDateInMillisSinceEpoch =
                    DataReductionProxySettings.getInstance().getDataReductionLastUpdateTime()
                    - DateUtils.DAY_IN_MILLIS * ChartDataUsageView.MAXIMUM_DAYS_IN_CHART;
            long firstEnabledInMillisSinceEpoch = DataReductionProxySettings.getInstance()
                                                          .getDataReductionProxyFirstEnabledTime();
            long mostRecentTime = chartStartDateInMillisSinceEpoch > firstEnabledInMillisSinceEpoch
                    ? chartStartDateInMillisSinceEpoch
                    : firstEnabledInMillisSinceEpoch;

            final int flags = DateUtils.FORMAT_ABBREV_MONTH | DateUtils.FORMAT_NO_YEAR;
            String date = DateUtils.formatDateTime(getContext(), mostRecentTime, flags).toString();

            itemText.setText(
                    getContext().getString(R.string.data_reduction_saved_label, dataSaved));
            itemSummary.setText(getContext().getString(R.string.data_reduction_date_label, date));
        } else {
            DataReductionProxyUma.dataReductionProxyUIAction(
                    DataReductionProxyUma.ACTION_MAIN_MENU_DISPLAYED_OFF);

            itemText.setText(R.string.data_reduction_title_lite_mode);
            itemSummary.setText(R.string.text_off);
        }

        setOnClickListener(this);
    }

    @Override
    public void onClick(View v) {
        RecordUserAction.record("MobileMenuDataSaverOpened");
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putBoolean(DataReductionPreferenceFragment.FROM_MAIN_MENU, true);
        PreferencesLauncher.launchSettingsPage(
                getContext(), DataReductionPreferenceFragment.class, fragmentArgs);

        Tracker tracker = TrackerFactory.getTrackerForProfile(Profile.getLastUsedProfile());
        tracker.notifyEvent(EventConstants.DATA_SAVER_DETAIL_OPENED);
    }
}
