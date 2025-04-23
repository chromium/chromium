// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.metrics;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.metrics.HistogramEventProtos.HistogramEventProto;

import java.util.HashSet;
import java.util.Set;

/**
 * Keeps a list of which histograms to upload if histograms filtering is applied.
 *
 * <p>The list below contains histograms that will be sampled at 100%. This is not done for all
 * histograms in order to preserve network usage and storage. See
 * go/clank-webview-uma#histograms-allowlist-guidance for reasons to add your histogram to it and
 * how to do it safely.
 */
@NullMarked
public class HistogramsAllowlist {
    private final Set<Long> mHistogramNameHashes;

    private HistogramsAllowlist(Set<Long> hashes) {
        mHistogramNameHashes = hashes;
    }

    public static HistogramsAllowlist load() {
        String[] histogramsAllowlist =
                new String[] {
                    // histograms_allowlist_check START_PARSING
                    "Android.WebView.HistoricalApplicationExitInfo.Counts2.FOREGROUND",
                    "Android.WebView.SafeMode.ActionName",
                    "Android.WebView.SafeMode.ReceivedFix",
                    "Android.WebView.SafeMode.SafeModeEnabled",
                    "Android.WebView.SitesVisitedWeekly",
                    "Android.WebView.Startup.CreationTime.Stage1.FactoryInit",
                    "Android.WebView.Startup.CreationTime.StartChromiumLocked",
                    "Android.WebView.Startup.CreationTime.TotalFactoryInitTime",
                    "Android.WebView.Visibility.Global",
                    "Android.WebView.VisibleScreenCoverage.PerWebView.data",
                    "Android.WebView.VisibleScreenCoverage.PerWebView.file",
                    "Android.WebView.VisibleScreenCoverage.PerWebView.http",
                    "Android.WebView.VisibleScreenCoverage.PerWebView.https",
                    "Autofill.WebView.Enabled",
                    "Memory.Total.PrivateMemoryFootprint",
                    "PageLoad.InteractiveTiming.InputDelay3",
                    "PageLoad.InteractiveTiming.UserInteractionLatency.HighPercentile2.MaxEventDuration",
                    "PageLoad.PaintTiming.NavigationToFirstContentfulPaint",
                    "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2",
                    "Power.ForegroundBatteryDrain.30SecondsAvg2",
                    // histograms_allowlist_check END_PARSING
                };
        Set<Long> hashes = new HashSet();
        for (String histogram : histogramsAllowlist) {
            hashes.add(AwMetricsUtils.hashHistogramName(histogram));
        }

        return new HistogramsAllowlist(hashes);
    }

    public boolean contains(Long histogramNameHash) {
        return mHistogramNameHashes.contains(histogramNameHash);
    }

    public boolean contains(HistogramEventProto histogramEventProto) {
        return mHistogramNameHashes.contains(histogramEventProto.getNameHash());
    }
}
