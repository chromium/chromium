// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/interstitials/delayed_interstitial_reporter.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/enterprise/data_protection/data_protection_features.h"
#include "chrome/browser/interstitials/enterprise_util.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace {
constexpr base::TimeDelta kTimeout = base::Seconds(5);
}

namespace enterprise_data_protection {

WEB_CONTENTS_USER_DATA_KEY_IMPL(DelayedInterstitialReporter);

// static
void DelayedInterstitialReporter::Start(content::WebContents* web_contents,
                                        TitleCallback report_callback,
                                        bool is_bypassing_interstitial,
                                        std::string uma_suffix) {
  if (!web_contents) {
    return;
  }

  // If the document has already finished loading, report immediately.
  if (!is_bypassing_interstitial &&
      web_contents->IsDocumentOnLoadCompletedInPrimaryMainFrame()) {
    std::string tab_title = base::UTF16ToUTF8(web_contents->GetTitle());

    base::UmaHistogramTimes(
        "Enterprise.DelayedReportingInterstitial.Time." + uma_suffix,
        base::TimeDelta());
    base::UmaHistogramBoolean(
        "Enterprise.DelayedReportingInterstitial.Timeout." + uma_suffix, false);

    std::move(report_callback).Run(tab_title);
    return;
  }

  // Otherwise, instantiate a self-deleting observer to wait for DidFinishLoad.
  DelayedInterstitialReporter::CreateForWebContents(
      web_contents, std::move(report_callback), is_bypassing_interstitial,
      uma_suffix);
}



DelayedInterstitialReporter::DelayedInterstitialReporter(
    content::WebContents* web_contents,
    TitleCallback report_callback,
    bool is_bypassing_interstitial,
    std::string uma_suffix)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<DelayedInterstitialReporter>(*web_contents),
      report_callback_(std::move(report_callback)),
      uma_suffix_(uma_suffix),
      start_time_(base::TimeTicks::Now()),
      is_bypassing_interstitial_(is_bypassing_interstitial) {
  timer_.Start(FROM_HERE, kTimeout,
               base::BindOnce(&DelayedInterstitialReporter::OnTimeout,
                              base::Unretained(this)));
}

void DelayedInterstitialReporter::OnTimeout() {
  RunCallbackAndCleanUp(RunState::kTimeout);
}

DelayedInterstitialReporter::~DelayedInterstitialReporter() {
  if (report_callback_) {
    std::string tab_title;
    if (web_contents()) {
      tab_title = base::UTF16ToUTF8(web_contents()->GetTitle());
    }
    std::move(report_callback_).Run(tab_title.substr(0, 1024));
  }
}

void DelayedInterstitialReporter::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (!render_frame_host->IsInPrimaryMainFrame()) {
    return;
  }
  RunCallbackAndCleanUp(RunState::kSuccess);
}

void DelayedInterstitialReporter::PrimaryPageChanged(content::Page& page) {
  if (is_bypassing_interstitial_) {
    is_bypassing_interstitial_ = false;
    return;
  }
  RunCallbackAndCleanUp(RunState::kFailed);
}


void DelayedInterstitialReporter::RunCallbackAndCleanUp(RunState run_state) {
  if (report_callback_) {
    std::string tab_title;
    if (web_contents()) {
      tab_title = base::UTF16ToUTF8(web_contents()->GetTitle());
    }

    switch (run_state) {
      case RunState::kSuccess:
        base::UmaHistogramTimes(
            "Enterprise.DelayedReportingInterstitial.Time." + uma_suffix_,
            base::TimeTicks::Now() - start_time_);
        [[fallthrough]];
      case RunState::kTimeout:
        base::UmaHistogramBoolean(
            "Enterprise.DelayedReportingInterstitial.Timeout." + uma_suffix_,
            run_state == RunState::kTimeout);
        break;
      case RunState::kFailed:
        break;
    }

    std::move(report_callback_).Run(tab_title.substr(0, 1024));
  }
  if (web_contents()) {
    web_contents()->RemoveUserData(UserDataKey());
  }
}
}  // namespace enterprise_data_protection
