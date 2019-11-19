// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/previews_ukm_observer.h"

#include "base/base64.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/previews/previews_content_util.h"
#include "chrome/browser/previews/previews_ui_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/previews_state.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source.h"

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
#include "chrome/browser/offline_pages/offline_page_tab_helper.h"
#endif

namespace previews {

namespace {

const char kOfflinePreviewsMimeType[] = "multipart/related";

bool ShouldOptionalEligibilityReasonBeRecorded(
    base::Optional<previews::PreviewsEligibilityReason> reason) {
  if (!reason.has_value())
    return false;

  // Do not record ALLOWED values since we are only interested in recording
  // reasons why a preview was not eligible to be shown.
  return reason.value() != previews::PreviewsEligibilityReason::ALLOWED;
}

}  // namespace

PreviewsUKMObserver::PreviewsUKMObserver()
    : committed_preview_(PreviewsType::NONE) {}

PreviewsUKMObserver::~PreviewsUKMObserver() {}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PreviewsUKMObserver::OnCommit(content::NavigationHandle* navigation_handle,
                              ukm::SourceId source_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  save_data_enabled_ = IsDataSaverEnabled(navigation_handle);

  PreviewsUITabHelper* ui_tab_helper =
      PreviewsUITabHelper::FromWebContents(navigation_handle->GetWebContents());
  if (!ui_tab_helper)
    return STOP_OBSERVING;

  previews::PreviewsUserData* previews_user_data =
      ui_tab_helper->GetPreviewsUserData(navigation_handle);
  if (!previews_user_data)
    return STOP_OBSERVING;

  committed_preview_ = previews_user_data->CommittedPreviewsType();

  // Only check for preview types that are decided before commit in the
  // |allowed_previews_state|.
  previews_likely_ =
      HasEnabledPreviews(previews_user_data->PreHoldbackAllowedPreviewsState() &
                         kPreCommitPreviews);
  content::PreviewsState previews_state =
      previews_user_data->PreHoldbackCommittedPreviewsState();

  // Check all preview types in the |committed_previews_state|. In practice
  // though, this will only set |previews_likely_| if it wasn't before for an
  // Optimization Hints preview.
  previews_likely_ |= HasEnabledPreviews(previews_state);

  if (navigation_handle->GetWebContents()->GetContentsMimeType() ==
      kOfflinePreviewsMimeType) {
    if (!IsOfflinePreview(navigation_handle->GetWebContents()))
      return STOP_OBSERVING;
    offline_preview_seen_ = true;
    DCHECK_EQ(previews::GetMainFramePreviewsType(previews_state),
              previews::PreviewsType::OFFLINE);
  }

  if (previews_state && previews::GetMainFramePreviewsType(previews_state) ==
                            previews::PreviewsType::LITE_PAGE) {
    lite_page_seen_ = true;
  }
  if (previews_state && previews::GetMainFramePreviewsType(previews_state) ==
                            previews::PreviewsType::LITE_PAGE_REDIRECT) {
    lite_page_redirect_seen_ = true;
  }
  if (previews_state && previews::GetMainFramePreviewsType(previews_state) ==
                            previews::PreviewsType::NOSCRIPT) {
    noscript_seen_ = true;
  }
  if (previews_state && previews::GetMainFramePreviewsType(previews_state) ==
                            previews::PreviewsType::RESOURCE_LOADING_HINTS) {
    resource_loading_hints_seen_ = true;
  }
  if (previews_state && previews::GetMainFramePreviewsType(previews_state) ==
                            previews::PreviewsType::DEFER_ALL_SCRIPT) {
    defer_all_script_seen_ = true;
  }
  if (previews_user_data->cache_control_no_transform_directive()) {
    origin_opt_out_occurred_ = true;
  }
  if (previews_user_data->server_lite_page_info()) {
    navigation_restart_penalty_ =
        navigation_handle->NavigationStart() -
        previews_user_data->server_lite_page_info()->original_navigation_start;
  }

  lite_page_eligibility_reason_ =
      previews_user_data->EligibilityReasonForPreview(
          previews::PreviewsType::LITE_PAGE);
  lite_page_redirect_eligibility_reason_ =
      previews_user_data->EligibilityReasonForPreview(
          previews::PreviewsType::LITE_PAGE_REDIRECT);
  noscript_eligibility_reason_ =
      previews_user_data->EligibilityReasonForPreview(
          previews::PreviewsType::NOSCRIPT);
  resource_loading_hints_eligibility_reason_ =
      previews_user_data->EligibilityReasonForPreview(
          previews::PreviewsType::RESOURCE_LOADING_HINTS);
  defer_all_script_eligibility_reason_ =
      previews_user_data->EligibilityReasonForPreview(
          previews::PreviewsType::DEFER_ALL_SCRIPT);
  offline_eligibility_reason_ = previews_user_data->EligibilityReasonForPreview(
      previews::PreviewsType::OFFLINE);

  serialized_hint_version_string_ =
      previews_user_data->serialized_hint_version_string();

  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PreviewsUKMObserver::ShouldObserveMimeType(const std::string& mime_type) const {
  if (PageLoadMetricsObserver::ShouldObserveMimeType(mime_type) ==
          CONTINUE_OBSERVING ||
      mime_type == kOfflinePreviewsMimeType) {
    return CONTINUE_OBSERVING;
  }
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PreviewsUKMObserver::OnStart(content::NavigationHandle* navigation_handle,
                             const GURL& currently_committed_url,
                             bool started_in_foreground) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!started_in_foreground)
    return STOP_OBSERVING;
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PreviewsUKMObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RecordMetrics();
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PreviewsUKMObserver::OnHidden(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RecordMetrics();
  return STOP_OBSERVING;
}

void PreviewsUKMObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RecordMetrics();
}

void PreviewsUKMObserver::RecordMetrics() {
  RecordPreviewsTypes();
  RecordOptimizationGuideInfo();
}

void PreviewsUKMObserver::RecordPreviewsTypes() {
  // Record the page end reason in UMA.
  if (committed_preview_ != PreviewsType::NONE) {
    UMA_HISTOGRAM_ENUMERATION(
        "Previews.PageEndReason", GetDelegate().GetPageEndReason(),
        page_load_metrics::PageEndReason::PAGE_END_REASON_COUNT);
  }
  base::UmaHistogramExactLinear(
      base::StringPrintf(
          "Previews.PageEndReason.%s",
          previews::GetStringNameForType(committed_preview_).c_str()),
      GetDelegate().GetPageEndReason(),
      page_load_metrics::PageEndReason::PAGE_END_REASON_COUNT);

  // Only record previews types when they are active.
  // |navigation_restart_penalty_| is included here because a Lite Page Redirect
  // preview can be attempted and not commit. This incurs the penalty but may
  // also cause no preview to be committed.
  if (!lite_page_seen_ && !noscript_seen_ && !resource_loading_hints_seen_ &&
      !defer_all_script_seen_ && !offline_preview_seen_ &&
      !origin_opt_out_occurred_ && !save_data_enabled_ &&
      !lite_page_redirect_seen_ && !navigation_restart_penalty_.has_value()) {
    return;
  }

  ukm::builders::Previews builder(GetDelegate().GetSourceId());
  if (lite_page_seen_)
    builder.Setlite_page(1);
  if (lite_page_redirect_seen_)
    builder.Setlite_page_redirect(1);
  if (noscript_seen_)
    builder.Setnoscript(1);
  if (resource_loading_hints_seen_)
    builder.Setresource_loading_hints(1);
  if (defer_all_script_seen_)
    builder.Setdefer_all_script(1);
  if (offline_preview_seen_)
    builder.Setoffline_preview(1);
  // 2 is set here for legacy reasons as it denotes an optout through the
  // omnibox ui as opposed to the now deprecated infobar.
  if (opt_out_occurred_)
    builder.Setopt_out(2);
  if (origin_opt_out_occurred_)
    builder.Setorigin_opt_out(1);
  if (save_data_enabled_)
    builder.Setsave_data_enabled(1);
  if (previews_likely_)
    builder.Setpreviews_likely(1);
  if (navigation_restart_penalty_.has_value()) {
    builder.Setnavigation_restart_penalty(
        navigation_restart_penalty_->InMilliseconds());
  }

  if (ShouldOptionalEligibilityReasonBeRecorded(
          lite_page_eligibility_reason_)) {
    builder.Setproxy_lite_page_eligibility_reason(
        static_cast<int>(lite_page_eligibility_reason_.value()));
  }
  if (ShouldOptionalEligibilityReasonBeRecorded(
          lite_page_redirect_eligibility_reason_)) {
    builder.Setlite_page_redirect_eligibility_reason(
        static_cast<int>(lite_page_redirect_eligibility_reason_.value()));
  }
  if (ShouldOptionalEligibilityReasonBeRecorded(noscript_eligibility_reason_)) {
    builder.Setnoscript_eligibility_reason(
        static_cast<int>(noscript_eligibility_reason_.value()));
  }
  if (ShouldOptionalEligibilityReasonBeRecorded(
          resource_loading_hints_eligibility_reason_)) {
    builder.Setresource_loading_hints_eligibility_reason(
        static_cast<int>(resource_loading_hints_eligibility_reason_.value()));
  }
  if (ShouldOptionalEligibilityReasonBeRecorded(
          defer_all_script_eligibility_reason_)) {
    builder.Setdefer_all_script_eligibility_reason(
        static_cast<int>(defer_all_script_eligibility_reason_.value()));
  }
  if (ShouldOptionalEligibilityReasonBeRecorded(offline_eligibility_reason_)) {
    builder.Setoffline_eligibility_reason(
        static_cast<int>(offline_eligibility_reason_.value()));
  }
  builder.Record(ukm::UkmRecorder::Get());
}

void PreviewsUKMObserver::RecordOptimizationGuideInfo() {
  if (!serialized_hint_version_string_.has_value()) {
    return;
  }

  // Deserialize the serialized version string into its protobuffer.
  std::string binary_version_pb;
  if (!base::Base64Decode(serialized_hint_version_string_.value(),
                          &binary_version_pb))
    return;

  optimization_guide::proto::Version hint_version;
  if (!hint_version.ParseFromString(binary_version_pb))
    return;

  ukm::builders::OptimizationGuide builder(GetDelegate().GetSourceId());
  if (hint_version.has_generation_timestamp() &&
      hint_version.generation_timestamp().seconds() > 0) {
    builder.SetHintGenerationTimestamp(
        hint_version.generation_timestamp().seconds());
  }
  if (hint_version.has_hint_source() &&
      hint_version.hint_source() !=
          optimization_guide::proto::HINT_SOURCE_UNKNOWN) {
    builder.SetHintSource(static_cast<int>(hint_version.hint_source()));
  }
  builder.Record(ukm::UkmRecorder::Get());
}

void PreviewsUKMObserver::OnEventOccurred(const void* const event_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (event_key == PreviewsUITabHelper::OptOutEventKey())
    opt_out_occurred_ = true;
}

bool PreviewsUKMObserver::IsDataSaverEnabled(
    content::NavigationHandle* navigation_handle) const {
  Profile* profile = Profile::FromBrowserContext(
      navigation_handle->GetWebContents()->GetBrowserContext());

  data_reduction_proxy::DataReductionProxySettings*
      data_reduction_proxy_settings =
          DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
              profile);
  if (!data_reduction_proxy_settings) {
    DCHECK(profile->IsOffTheRecord());
    return false;
  }

  return data_reduction_proxy_settings->IsDataReductionProxyEnabled();
}

bool PreviewsUKMObserver::IsOfflinePreview(
    content::WebContents* web_contents) const {
#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  offline_pages::OfflinePageTabHelper* tab_helper =
      offline_pages::OfflinePageTabHelper::FromWebContents(web_contents);
  return tab_helper && tab_helper->GetOfflinePreviewItem();
#else
  return false;
#endif
}

}  // namespace previews
