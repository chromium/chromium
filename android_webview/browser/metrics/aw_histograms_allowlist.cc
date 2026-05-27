// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// LINT.IfChange(AndroidWebViewHistogramsAllowlistPath)
// The path to this file is hardcoded in:
// tools/metrics/histograms/histograms_allowlist_check.py
// LINT.ThenChange(//tools/metrics/histograms/histograms_allowlist_check.py:AndroidWebViewHistogramsAllowlistPath)

#include "android_webview/browser/metrics/aw_histograms_allowlist.h"

#include "base/metrics/metrics_hashes.h"
#include "base/no_destructor.h"

namespace android_webview {

namespace {

const char* const kHistogramsAllowlist[] = {
    // clang-format off
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
    // clang-format on
};

}  // namespace

// static
AwHistogramsAllowlist* AwHistogramsAllowlist::GetInstance() {
  static base::NoDestructor<AwHistogramsAllowlist> instance;
  return instance.get();
}

AwHistogramsAllowlist::AwHistogramsAllowlist() {
  for (const char* name : kHistogramsAllowlist) {
    allowlist_hashes_.insert(base::HashMetricName(name));
  }
}

AwHistogramsAllowlist::~AwHistogramsAllowlist() = default;

bool AwHistogramsAllowlist::Contains(uint64_t hash) const {
  return allowlist_hashes_.contains(hash);
}

}  // namespace android_webview
