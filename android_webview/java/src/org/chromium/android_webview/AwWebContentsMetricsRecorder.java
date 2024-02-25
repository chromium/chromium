// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.android_webview.settings.ForceDarkBehavior;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.content_public.browser.LoadCommittedDetails;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;

import java.lang.ref.WeakReference;
import java.util.Set;

/**
 * This class records WebView settings usage.
 *
 * It records histograms at navigationEntryCommitted to show what the settings were on navigation.
 * It also offers static helpers to record the settings as they are configured by the embedding
 * application.
 */
@Lifetime.WebView
public class AwWebContentsMetricsRecorder extends WebContentsObserver {
    private WeakReference<Context> mContext;
    private WeakReference<AwSettings> mAwSettings;

    public AwWebContentsMetricsRecorder(
            WebContents webContents, Context context, AwSettings awSettings) {
        super(webContents);
        mContext = new WeakReference<Context>(context);
        mAwSettings = new WeakReference<AwSettings>(awSettings);
    }

    @Override
    public void navigationEntryCommitted(LoadCommittedDetails details) {
        if (!details.isMainFrame()) return;
        recordDarkModeMetrics();
        recordRequestedWithHeaderMetrics();
    }

    private void recordDarkModeMetrics() {
        Context context = mContext.get();
        if (context == null) return;

        AwSettings awSettings = mAwSettings.get();
        if (awSettings == null) return;

        int nightMode = DarkModeHelper.getNightMode(context);
        int lightTheme = DarkModeHelper.getLightTheme(context);
        boolean isForceDarkApplied = awSettings.isForceDarkApplied();
        int forceDarkMode = awSettings.getForceDarkMode();
        int forceDarkBehavior = awSettings.getForceDarkBehavior();
        int textLuminance = DarkModeHelper.getPrimaryTextLuminace(context);
        recordDarkModeMetrics(
                nightMode,
                lightTheme,
                isForceDarkApplied,
                forceDarkMode,
                forceDarkBehavior,
                textLuminance);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static void recordDarkModeMetrics(
            int nightMode,
            int lightTheme,
            boolean isForceDarkApplied,
            int forceDarkMode,
            int forceDarkBehavior,
            int textLuminance) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.WebView.DarkMode.ForceDarkBehavior",
                forceDarkBehavior,
                AwSettings.FORCE_DARK_STRATEGY_COUNT);
        RecordHistogram.recordEnumeratedHistogram(
                "Android.WebView.DarkMode.ForceDarkMode",
                forceDarkMode,
                AwSettings.FORCE_DARK_MODES_COUNT);
        RecordHistogram.recordBooleanHistogram(
                "Android.WebView.DarkMode.InDarkMode", isForceDarkApplied);
        // Refer to WebViewInDarkModeVsLightTheme in enums.xml.
        RecordHistogram.recordEnumeratedHistogram(
                "Android.WebView.DarkMode.InDarkModeVsLightTheme",
                (isForceDarkApplied ? 0 : 1) * DarkModeHelper.LightTheme.LIGHT_THEME_COUNT
                        + lightTheme,
                2 * DarkModeHelper.LightTheme.LIGHT_THEME_COUNT);
        // Refer to WebViewInDarkModeVsNightMode in enums.xml.
        RecordHistogram.recordEnumeratedHistogram(
                "Android.WebView.DarkMode.InDarkModeVsNightMode",
                (isForceDarkApplied ? 0 : 1) * DarkModeHelper.NightMode.NIGHT_MODE_COUNT
                        + nightMode,
                2 * DarkModeHelper.NightMode.NIGHT_MODE_COUNT);
        RecordHistogram.recordEnumeratedHistogram(
                "Android.WebView.DarkMode.LightTheme",
                lightTheme,
                DarkModeHelper.LightTheme.LIGHT_THEME_COUNT);
        RecordHistogram.recordEnumeratedHistogram(
                "Android.WebView.DarkMode.PrimaryTextLuminanceVsLightTheme",
                textLuminance * DarkModeHelper.LightTheme.LIGHT_THEME_COUNT + lightTheme,
                DarkModeHelper.TextLuminance.TEXT_LUMINACE_COUNT
                        * DarkModeHelper.LightTheme.LIGHT_THEME_COUNT);
        RecordHistogram.recordEnumeratedHistogram(
                "Android.WebView.DarkMode.PrimaryTextLuminanceVsNightMode",
                textLuminance * DarkModeHelper.NightMode.NIGHT_MODE_COUNT + nightMode,
                DarkModeHelper.TextLuminance.TEXT_LUMINACE_COUNT
                        * DarkModeHelper.NightMode.NIGHT_MODE_COUNT);
        RecordHistogram.recordEnumeratedHistogram(
                "Android.WebView.DarkMode.NightMode",
                nightMode,
                DarkModeHelper.NightMode.NIGHT_MODE_COUNT);
        RecordHistogram.recordEnumeratedHistogram(
                "Android.WebView.DarkMode.NightModeVsLightTheme",
                nightMode * DarkModeHelper.LightTheme.LIGHT_THEME_COUNT + lightTheme,
                DarkModeHelper.NightMode.NIGHT_MODE_COUNT
                        * DarkModeHelper.LightTheme.LIGHT_THEME_COUNT);
    }

    private void recordRequestedWithHeaderMetrics() {
        AwSettings awSettings = mAwSettings.get();
        if (awSettings == null) return;
        Set<String> allowList = awSettings.getRequestedWithHeaderOriginAllowList();
        RecordHistogram.recordCount1000Histogram(
                "Android.WebView.RequestedWithHeader.OnNavigationRequestedWithHeaderAllowListSize",
                allowList.size());
    }

    public static void recordForceDarkModeAPIUsage(Context context, int forceDarkMode) {
        int value =
                DarkModeHelper.getNightMode(context) * AwSettings.FORCE_DARK_MODES_COUNT
                        + forceDarkMode;
        System.out.println("recordForce value " + value);
        RecordHistogram.recordEnumeratedHistogram(
                "Android.WebView.ForceDarkMode",
                value,
                DarkModeHelper.NightMode.NIGHT_MODE_COUNT * AwSettings.FORCE_DARK_MODES_COUNT);
    }

    public static void recordForceDarkBehaviorAPIUsage(@ForceDarkBehavior int forceDarkBehavior) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.WebView.ForceDarkBehavior",
                forceDarkBehavior,
                AwSettings.FORCE_DARK_STRATEGY_COUNT);
    }

    public static void recordRequestedWithHeaderModeAPIUsage(@NonNull Set<String> originAllowList) {
        RecordHistogram.recordCount1000Histogram(
                "Android.WebView.RequestedWithHeader.SetRequestedWithHeaderModeAllowListSize",
                originAllowList.size());
    }

    public static void recordRequestedWithHeaderModeServiceWorkerAPIUsage(
            @NonNull Set<String> originAllowList) {
        RecordHistogram.recordCount1000Histogram(
                "Android.WebView.RequestedWithHeader."
                        + "SetServiceWorkerRequestedWithHeaderModeAllowListSize",
                originAllowList.size());
    }
}
