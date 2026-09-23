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

// Maximum number of bytes reported for a tab title. Kept in sync with the
// truncation the reporting pipeline applies in
// enterprise_connectors::MaybeTruncateLongUrls().
constexpr size_t kMaxTabTitleBytes = 1024;

// Returns the current tab title of `web_contents`, truncated on a UTF-8 code
// point boundary. Returns an empty string if there is no WebContents.
std::string GetTruncatedTabTitle(content::WebContents* web_contents) {
  if (!web_contents) {
    return std::string();
  }
  return std::string(base::TruncateUTF8ToByteSize(
      base::UTF16ToUTF8(web_contents->GetTitle()), kMaxTabTitleBytes));
}

}  // namespace

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
    base::UmaHistogramTimes(
        "Enterprise.DelayedReportingInterstitial.Time." + uma_suffix,
        base::TimeDelta());
    base::UmaHistogramBoolean(
        "Enterprise.DelayedReportingInterstitial.Timeout." + uma_suffix, false);

    std::move(report_callback).Run(GetTruncatedTabTitle(web_contents));
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
    std::move(report_callback_).Run(GetTruncatedTabTitle(web_contents()));
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
    std::string tab_title = GetTruncatedTabTitle(web_contents());

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
        // TODO(crbug.com/467657459): the primary page changed before the page
        // being reported on finished loading, so the sampled title may describe
        // a different document. No histogram is recorded here, so the rate of
        // this path is currently unknown; add one before deciding whether to
        // report an empty title instead.
        break;
    }

    std::move(report_callback_).Run(tab_title);
  }
  if (web_contents()) {
    web_contents()->RemoveUserData(UserDataKey());
  }
}
}  // namespace enterprise_data_protection
