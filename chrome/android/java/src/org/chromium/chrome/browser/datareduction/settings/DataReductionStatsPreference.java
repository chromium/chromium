// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.datareduction.settings;

import static android.text.format.DateUtils.FORMAT_ABBREV_MONTH;
import static android.text.format.DateUtils.FORMAT_NO_YEAR;
import static android.text.format.DateUtils.FORMAT_SHOW_DATE;

import static org.chromium.third_party.android.datausagechart.ChartDataUsageView.MAXIMUM_DAYS_IN_CHART;
import static org.chromium.third_party.android.datausagechart.ChartDataUsageView.MINIMUM_DAYS_IN_CHART;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.DialogInterface;
import android.text.format.DateUtils;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.AlertDialog;
import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;
import androidx.vectordrawable.graphics.drawable.VectorDrawableCompat;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.datareduction.DataReductionProxyUma;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.settings.datareduction.DataReductionProxySavingsClearedReason;
import org.chromium.chrome.browser.util.FileSizeUtil;
import org.chromium.components.browser_ui.util.ConversionUtils;
import org.chromium.third_party.android.datausagechart.ChartDataUsageView;
import org.chromium.third_party.android.datausagechart.NetworkStats;
import org.chromium.third_party.android.datausagechart.NetworkStatsHistory;
import org.chromium.ui.widget.RectProvider;
import org.chromium.ui.widget.ViewRectProvider;

import java.util.List;
import java.util.TimeZone;

/**
 * Preference used to display statistics on data reduction.
 */
public class DataReductionStatsPreference extends Preference {
    private static final String TAG = "DataSaverStats";

    private NetworkStatsHistory mOriginalNetworkStatsHistory;
    private NetworkStatsHistory mReceivedNetworkStatsHistory;
    private List<DataReductionDataUseItem> mSiteBreakdownItems;

    private ViewRectProvider mDataReductionStatsPreferenceViewRectProvider;
    private LinearLayout mDataReductionStatsContainer;
    private TextView mInitialDataSavingsTextView;
    private TextView mDataSavingsTextView;
    private TextView mDataUsageTextView;
    private TextView mStartDateTextView;
    private TextView mEndDateTextView;
    private Button mResetStatisticsButton;
    private ChartDataUsageView mChartDataUsageView;
    private DataReductionSiteBreakdownView mDataReductionBreakdownView;
    private boolean mShouldShowRealData;
    private boolean mIsFirstDayChart;
    /** Number of days that the chart will present. */
    private int mNumDaysInChart;
    /** Number of milliseconds from the beginning of the day. */
    private long mTimeOfDayOffsetMillis;
    private long mVisibleStartTimeMillis;
    private long mVisibleEndTimeMillis;
    private CharSequence mSavingsTotalPhrase;
    private CharSequence mReceivedTotalPhrase;
    private String mStartDatePhrase;
    private String mEndDatePhrase;

    /**
     * If this is the first time the site breakdown feature is enabled, set the first allowable date
     * the breakdown can be shown.
     */
    public static void initializeDataReductionSiteBreakdownPref() {
        // If the site breakdown pref has already been set, don't set it.
        if (SharedPreferencesManager.getInstance().contains(
                    ChromePreferenceKeys.DATA_REDUCTION_SITE_BREAKDOWN_ALLOWED_DATE)) {
            return;
        }

        long lastUpdateTimeMillis =
                DataReductionProxySettings.getInstance().getDataReductionLastUpdateTime();

        // If the site breakdown is enabled and there are historical stats within the last
        // MAXIMUM_DAYS_IN_CHART days, don't show the breakdown for another MAXIMUM_DAYS_IN_CHART
        // days from the last update time. Otherwise, the site breakdown can be shown starting now.
        long timeChartCanBeShown =
                lastUpdateTimeMillis + MAXIMUM_DAYS_IN_CHART * DateUtils.DAY_IN_MILLIS;
        long now = System.currentTimeMillis();
        SharedPreferencesManager.getInstance().writeLong(
                ChromePreferenceKeys.DATA_REDUCTION_SITE_BREAKDOWN_ALLOWED_DATE,
                timeChartCanBeShown > now ? timeChartCanBeShown : now);
    }

    public DataReductionStatsPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        setWidgetLayoutResource(R.layout.data_reduction_stats_layout);
    }

    /**
     * Updates the preference screen to convey current statistics on data reduction.
     */
    public void updateReductionStatistics(long currentTimeMillis) {
        long original[] = DataReductionProxySettings.getInstance().getOriginalNetworkStatsHistory();
        long received[] = DataReductionProxySettings.getInstance().getReceivedNetworkStatsHistory();

        long dataSaverEnabledDayMillis =
                (DataReductionProxySettings.getInstance().getDataReductionProxyFirstEnabledTime()
                        / DateUtils.DAY_IN_MILLIS)
                * DateUtils.DAY_IN_MILLIS;
        long dataSaverEnabledTimeMillis =
                DataReductionProxySettings.getInstance().getDataReductionProxyFirstEnabledTime();
        long startOfToday = currentTimeMillis - currentTimeMillis % DateUtils.DAY_IN_MILLIS
                - TimeZone.getDefault().getOffset(currentTimeMillis);
        long statsLastUpdateTimeMillis =
                DataReductionProxySettings.getInstance().getDataReductionLastUpdateTime();
        if (statsLastUpdateTimeMillis == 0) {
            // Use startOfToday if stats were recently reset.
            statsLastUpdateTimeMillis = startOfToday;
        }
        Long numDaysSinceStatsUpdated = (statsLastUpdateTimeMillis < startOfToday)
                ? (startOfToday - statsLastUpdateTimeMillis) / DateUtils.DAY_IN_MILLIS
                : 0;

        // Capture the offset from the start of today until the current time for determining the
        // day label to present later (handles timezone adjustment).
        mTimeOfDayOffsetMillis = currentTimeMillis - startOfToday;

        // Determine the start and end time of the network stats to chart.
        Long daysEnabled =
                (currentTimeMillis - dataSaverEnabledDayMillis) / DateUtils.DAY_IN_MILLIS + 1;
        mIsFirstDayChart = false;
        mNumDaysInChart = MAXIMUM_DAYS_IN_CHART;
        if (daysEnabled < MINIMUM_DAYS_IN_CHART) {
            mIsFirstDayChart = true;
            mNumDaysInChart = MINIMUM_DAYS_IN_CHART;
        } else if (daysEnabled < MAXIMUM_DAYS_IN_CHART) {
            mNumDaysInChart = daysEnabled.intValue();
        }
        mOriginalNetworkStatsHistory = getNetworkStatsHistory(original, mNumDaysInChart);
        mReceivedNetworkStatsHistory = getNetworkStatsHistory(received, mNumDaysInChart);

        mShouldShowRealData =
                ConversionUtils.bytesToKilobytes(mReceivedNetworkStatsHistory.getTotalBytes())
                >= DataReductionProxySettings.DATA_REDUCTION_SHOW_CHART_KB_THRESHOLD;

        // Determine the visible start and end points based on the available data and when it was
        // last updated.
        mVisibleStartTimeMillis = mOriginalNetworkStatsHistory.getStart()
                + numDaysSinceStatsUpdated.intValue() * DateUtils.DAY_IN_MILLIS
                + DateUtils.DAY_IN_MILLIS;
        mVisibleEndTimeMillis = mOriginalNetworkStatsHistory.getEnd()
                + numDaysSinceStatsUpdated.intValue() * DateUtils.DAY_IN_MILLIS;

        if (mShouldShowRealData && mDataReductionBreakdownView != null
                && currentTimeMillis > SharedPreferencesManager.getInstance().readLong(
                           ChromePreferenceKeys.DATA_REDUCTION_SITE_BREAKDOWN_ALLOWED_DATE,
                           Long.MAX_VALUE)) {
            DataReductionProxySettings.getInstance().queryDataUsage(
                    mNumDaysInChart, new Callback<List<DataReductionDataUseItem>>() {
                        @Override
                        public void onResult(List<DataReductionDataUseItem> result) {
                            mSiteBreakdownItems = result;

                            mDataReductionBreakdownView.setAndDisplayDataUseItems(
                                    mSiteBreakdownItems);
                        }
                    });
        }
    }

    /**
     * Returns the number of days that the data usage graph should display based on the last
     * update to the statistics.
     */
    @VisibleForTesting
    int getNumDaysInChart() {
        return mNumDaysInChart;
    }

    @VisibleForTesting
    boolean shouldShowRealData() {
        return mShouldShowRealData;
    }

    private static NetworkStatsHistory getNetworkStatsHistory(long[] history, int days) {
        if (days > history.length) days = history.length;
        NetworkStatsHistory networkStatsHistory = new NetworkStatsHistory(
                DateUtils.DAY_IN_MILLIS, days, NetworkStatsHistory.FIELD_RX_BYTES);

        DataReductionProxySettings config = DataReductionProxySettings.getInstance();
        long time = config.getDataReductionLastUpdateTime() - days * DateUtils.DAY_IN_MILLIS;
        for (int i = history.length - days, bucket = 0; i < history.length; i++, bucket++) {
            NetworkStats.Entry entry = new NetworkStats.Entry();
            entry.rxBytes = Math.max(history[i], 0);
            long startTime = time + (DateUtils.DAY_IN_MILLIS * bucket);
            // Spread each day's record over the first hour of the day.
            networkStatsHistory.recordData(startTime, startTime + DateUtils.HOUR_IN_MILLIS, entry);
        }
        return networkStatsHistory;
    }

    private void updateDetailView() {
        final Context context = getContext();

        // updateDetailData also updates some UMA based on the actual data shown, so only update it
        // if we are actually showing the chart.
        if (mShouldShowRealData) updateDetailData();

        mInitialDataSavingsTextView.setVisibility(mShouldShowRealData ? View.GONE : View.VISIBLE);
        mDataReductionStatsContainer.setVisibility(mShouldShowRealData ? View.VISIBLE : View.GONE);

        mStartDateTextView.setText(mShouldShowRealData ? mStartDatePhrase : "");
        mStartDateTextView.setContentDescription(mShouldShowRealData
                        ? context.getString(R.string.data_reduction_start_date_content_description,
                                mStartDatePhrase)
                        : "");
        mEndDateTextView.setText(mShouldShowRealData ? mEndDatePhrase : "");
        mEndDateTextView.setContentDescription(mShouldShowRealData
                        ? context.getString(R.string.data_reduction_end_date_content_description,
                                mEndDatePhrase)
                        : "");
        if (mDataUsageTextView != null) {
            mDataUsageTextView.setText(mShouldShowRealData ? mReceivedTotalPhrase : "");
        }
        if (mDataSavingsTextView != null) {
            mDataSavingsTextView.setText(mShouldShowRealData ? mSavingsTotalPhrase : "");
        }
    }

    /**
     * Keep the graph labels LTR oriented. In RTL languages, numbers and plots remain LTR.
     */
    @SuppressLint("RtlHardcoded")
    private void forceLayoutGravityOfGraphLabels() {
        ((FrameLayout.LayoutParams) mStartDateTextView.getLayoutParams()).gravity = Gravity.LEFT;
        ((FrameLayout.LayoutParams) mEndDateTextView.getLayoutParams()).gravity = Gravity.RIGHT;
    }

    /**
     * Initializes a view rect observer to listen for when the bounds of the view has changed, so we
     * can update the minimum height of the view accordingly.
     *
     * @param view The view to listen for bounds changes on.
     */
    private void initializeViewBounds(final View view) {
        if (mDataReductionStatsPreferenceViewRectProvider != null) {
            mDataReductionStatsPreferenceViewRectProvider.stopObserving();
        }
        mDataReductionStatsPreferenceViewRectProvider = new ViewRectProvider(view);
        mDataReductionStatsPreferenceViewRectProvider.startObserving(new RectProvider.Observer() {
            @Override
            public void onRectChanged() {
                int screenHeight = getContext().getResources().getDisplayMetrics().heightPixels;
                int offset = mDataReductionStatsPreferenceViewRectProvider.getRect().top;
                view.setMinimumHeight(screenHeight - offset);
            }

            @Override
            public void onRectHidden() {}
        });
    }

    /**
     * Sets up a data usage chart and text views containing data reduction statistics.
     * @param holder The current view holder.
     */
    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        initializeViewBounds(holder.itemView);

        mInitialDataSavingsTextView = (TextView) holder.findViewById(R.id.initial_data_savings);
        mInitialDataSavingsTextView.setCompoundDrawablesWithIntrinsicBounds(null,
                VectorDrawableCompat.create(getContext().getResources(),
                        R.drawable.data_reduction_big, getContext().getTheme()),
                null, null);

        mDataReductionStatsContainer =
                (LinearLayout) holder.findViewById(R.id.data_reduction_stats_container);
        mDataUsageTextView = (TextView) holder.findViewById(R.id.data_reduction_usage);
        mDataSavingsTextView = (TextView) holder.findViewById(R.id.data_reduction_savings);
        mStartDateTextView = (TextView) holder.findViewById(R.id.data_reduction_start_date);
        mEndDateTextView = (TextView) holder.findViewById(R.id.data_reduction_end_date);
        mDataReductionBreakdownView =
                (DataReductionSiteBreakdownView) holder.findViewById(R.id.breakdown);
        forceLayoutGravityOfGraphLabels();
        if (mOriginalNetworkStatsHistory == null) {
            // This will query data usage. Only set mSiteBreakdownItems if the statistics are not
            // being queried.
            updateReductionStatistics(System.currentTimeMillis());
        } else if (mSiteBreakdownItems != null && mShouldShowRealData) {
            mDataReductionBreakdownView.setAndDisplayDataUseItems(mSiteBreakdownItems);
        }

        mChartDataUsageView = (ChartDataUsageView) holder.findViewById(R.id.chart);
        mChartDataUsageView.bindNetworkStats(
                mOriginalNetworkStatsHistory, mReceivedNetworkStatsHistory);
        mChartDataUsageView.setVisibleRange(mVisibleStartTimeMillis, mVisibleEndTimeMillis);

        if (DataReductionProxySettings.getInstance().isDataReductionProxyUnreachable()) {
            // Leave breadcrumb in log for user feedback report.
            Log.w(TAG, "Data Saver proxy unreachable when user viewed Data Saver stats");
        }

        mResetStatisticsButton = (Button) holder.findViewById(R.id.data_reduction_reset_statistics);
        if (mResetStatisticsButton != null) {
            setUpResetStatisticsButton();
        }

        updateDetailView();
    }

    private void setUpResetStatisticsButton() {
        mResetStatisticsButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View view) {
                DialogInterface.OnClickListener dialogListener = new AlertDialog.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        if (which == AlertDialog.BUTTON_POSITIVE) {
                            // If the site breakdown hasn't been shown yet because there was
                            // historical data, reset that state so that the site breakdown can
                            // now be shown.
                            long now = System.currentTimeMillis();
                            if (SharedPreferencesManager.getInstance().readLong(
                                        ChromePreferenceKeys
                                                .DATA_REDUCTION_SITE_BREAKDOWN_ALLOWED_DATE,
                                        Long.MAX_VALUE)
                                    > now) {
                                SharedPreferencesManager.getInstance().writeLong(
                                        ChromePreferenceKeys
                                                .DATA_REDUCTION_SITE_BREAKDOWN_ALLOWED_DATE,
                                        now);
                            }
                            DataReductionProxySettings.getInstance().clearDataSavingStatistics(
                                    DataReductionProxySavingsClearedReason
                                            .USER_ACTION_SETTINGS_MENU);
                            updateReductionStatistics(now);
                            updateDetailView();
                            notifyChanged();
                            DataReductionProxyUma.dataReductionProxyUIAction(
                                    DataReductionProxyUma.ACTION_STATS_RESET);
                        } else {
                            // Do nothing if canceled.
                        }
                    }
                };

                final int title =
                        R.string.data_reduction_usage_reset_statistics_confirmation_title_lite_mode;
                final int message =
                        R.string.data_reduction_usage_reset_statistics_confirmation_dialog_lite_mode;
                new AlertDialog.Builder(getContext(), R.style.Theme_Chromium_AlertDialog)
                        .setTitle(title)
                        .setMessage(message)
                        .setPositiveButton(
                                R.string.data_reduction_usage_reset_statistics_confirmation_button,
                                dialogListener)
                        .setNegativeButton(R.string.cancel, dialogListener)
                        .show();
            }
        });
    }

    /**
     * Update data reduction statistics whenever the chart's inspection
     * range changes. In particular, this creates strings describing the total
     * original size of all data received over the date range, the total size
     * of all data received (after compression), and the percent data reduction
     * and the range of dates over which these statistics apply.
     */
    private void updateDetailData() {
        // To determine the correct day labels, adjust the network stats time values by their
        // offset from the client's current time.
        final long startDay = mVisibleStartTimeMillis + mTimeOfDayOffsetMillis;
        final long endDay = mVisibleEndTimeMillis + mTimeOfDayOffsetMillis;
        final Context context = getContext();

        final long compressedTotalBytes = mReceivedNetworkStatsHistory.getTotalBytes();
        mReceivedTotalPhrase = FileSizeUtil.formatFileSize(context, compressedTotalBytes);
        final long originalTotalBytes = mOriginalNetworkStatsHistory.getTotalBytes();
        final long savingsTotalBytes = Math.max(originalTotalBytes - compressedTotalBytes, 0);
        mSavingsTotalPhrase = FileSizeUtil.formatFileSize(context, savingsTotalBytes);
        if (mIsFirstDayChart) {
            // Only show the current date on the left hand side for the single-day-chart.
            mStartDatePhrase = formatDate(context, endDay);
            mEndDatePhrase = null;
        } else {
            mStartDatePhrase = formatDate(context, startDay);
            mEndDatePhrase = formatDate(context, endDay);
        }

        DataReductionProxyUma.dataReductionProxyUserViewedSavings(
                compressedTotalBytes, originalTotalBytes);

        // Here to the end of the method checks for a difference in the reported data breakdown. If
        // mSiteBreakdownItems is null, return early.
        if (mSiteBreakdownItems == null) return;

        long breakdownSavingsTotal = 0;
        long breakdownUsageTotal = 0;
        for (DataReductionDataUseItem item : mSiteBreakdownItems) {
            breakdownSavingsTotal += item.getDataSaved();
            breakdownUsageTotal += item.getDataUsed();
        }
        final long savingsDiff = Math.abs(breakdownSavingsTotal - savingsTotalBytes);
        final long usageDiff = Math.abs(breakdownUsageTotal - compressedTotalBytes);
        final long savingsTotal = breakdownSavingsTotal + savingsTotalBytes;
        final long usageTotal = breakdownUsageTotal + compressedTotalBytes;

        if (savingsTotal <= 0 || usageTotal <= 0) return;

        final int savingsDiffPercent = (int) (savingsDiff / savingsTotal * 100);
        final int usageDiffPercent = (int) (usageDiff / usageTotal * 100);

        DataReductionProxyUma.dataReductionProxyUserViewedSavingsDifference(
                savingsDiffPercent, usageDiffPercent);
    }

    private static String formatDate(Context context, long millisSinceEpoch) {
        final int flags = FORMAT_SHOW_DATE | FORMAT_ABBREV_MONTH | FORMAT_NO_YEAR;
        return DateUtils.formatDateTime(context, millisSinceEpoch, flags).toString();
    }
}
