// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_ui_tab_helper.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/loader/chrome_navigation_data.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/page_load_metrics/metrics_web_contents_observer.h"
#include "chrome/browser/previews/previews_infobar_delegate.h"
#include "chrome/browser/previews/previews_service.h"
#include "chrome/browser/previews/previews_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/network_time/network_time_tracker.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/previews/content/previews_content_util.h"
#include "components/previews/content/previews_ui_service.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_response_headers.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
#include "chrome/browser/offline_pages/offline_page_tab_helper.h"
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)

namespace {

const void* const kOptOutEventKey = 0;

const char kMinStalenessParamName[] = "min_staleness_in_minutes";
const char kMaxStalenessParamName[] = "max_staleness_in_minutes";
const int kMinStalenessParamDefaultValue = 5;
const int kMaxStalenessParamDefaultValue = 1440;

// Adds the preview navigation to the black list.
void AddPreviewNavigationCallback(content::BrowserContext* browser_context,
                                  const GURL& url,
                                  previews::PreviewsType type,
                                  uint64_t page_id,
                                  bool opt_out) {
  PreviewsService* previews_service = PreviewsServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context));
  if (previews_service && previews_service->previews_ui_service()) {
    previews_service->previews_ui_service()->AddPreviewNavigation(
        url, type, opt_out, page_id);
  }
}

void RecordStaleness(PreviewsUITabHelper::PreviewsStalePreviewTimestamp value) {
  UMA_HISTOGRAM_ENUMERATION("Previews.StalePreviewTimestampShown", value);
}

void InformPLMOfOptOut(content::WebContents* web_contents) {
  page_load_metrics::MetricsWebContentsObserver* metrics_web_contents_observer =
      page_load_metrics::MetricsWebContentsObserver::FromWebContents(
          web_contents);
  if (!metrics_web_contents_observer)
    return;

  metrics_web_contents_observer->BroadcastEventToObservers(
      PreviewsUITabHelper::OptOutEventKey());
}

bool ShouldShowUIForPreviewsType(previews::PreviewsType type) {
  if (type == previews::PreviewsType::NONE)
    return false;

  // Show the UI for LoFi at commit if the UI is the Android Omnibox.
  if (type == previews::PreviewsType::LOFI)
    return previews::params::IsPreviewsOmniboxUiEnabled();

  return true;
}

}  // namespace

PreviewsUITabHelper::~PreviewsUITabHelper() {}

PreviewsUITabHelper::PreviewsUITabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents), weak_factory_(this) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

void PreviewsUITabHelper::ShowUIElement(
    previews::PreviewsType previews_type,
    bool is_data_saver_user,
    OnDismissPreviewsUICallback on_dismiss_callback) {
  // Retrieve PreviewsUIService* from |web_contents| if available.
  PreviewsService* previews_service = PreviewsServiceFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
  previews::PreviewsUIService* previews_ui_service =
      previews_service ? previews_service->previews_ui_service() : nullptr;

  on_dismiss_callback_ = std::move(on_dismiss_callback);

#if defined(OS_ANDROID)
  if (previews::params::IsPreviewsOmniboxUiEnabled()) {
    should_display_android_omnibox_badge_ = true;
    return;
  }
#endif

  PreviewsInfoBarDelegate::Create(web_contents(), previews_type,
                                  is_data_saver_user, previews_ui_service);
}

base::string16 PreviewsUITabHelper::GetStalePreviewTimestampText() {
  if (previews_freshness_.is_null())
    return base::string16();
  if (!base::FeatureList::IsEnabled(
          previews::features::kStalePreviewsTimestamp)) {
    return base::string16();
  }

  int min_staleness_in_minutes = base::GetFieldTrialParamByFeatureAsInt(
      previews::features::kStalePreviewsTimestamp, kMinStalenessParamName,
      kMinStalenessParamDefaultValue);
  int max_staleness_in_minutes = base::GetFieldTrialParamByFeatureAsInt(
      previews::features::kStalePreviewsTimestamp, kMaxStalenessParamName,
      kMaxStalenessParamDefaultValue);

  if (min_staleness_in_minutes <= 0 || max_staleness_in_minutes <= 0) {
    NOTREACHED();
    return base::string16();
  }

  base::Time network_time;
  if (g_browser_process->network_time_tracker()->GetNetworkTime(&network_time,
                                                                nullptr) !=
      network_time::NetworkTimeTracker::NETWORK_TIME_AVAILABLE) {
    // When network time has not been initialized yet, simply rely on the
    // machine's current time.
    network_time = base::Time::Now();
  }

  if (network_time < previews_freshness_) {
    RecordStaleness(
        PreviewsStalePreviewTimestamp::kTimestampNotShownStalenessNegative);
    return base::string16();
  }

  int staleness_in_minutes = (network_time - previews_freshness_).InMinutes();
  if (staleness_in_minutes < min_staleness_in_minutes) {
    if (is_stale_reload_) {
      RecordStaleness(PreviewsStalePreviewTimestamp::kTimestampUpdatedNowShown);
      return l10n_util::GetStringUTF16(
          IDS_PREVIEWS_INFOBAR_TIMESTAMP_UPDATED_NOW);
    }
    RecordStaleness(
        PreviewsStalePreviewTimestamp::kTimestampNotShownPreviewNotStale);
    return base::string16();
  }
  if (staleness_in_minutes > max_staleness_in_minutes) {
    RecordStaleness(PreviewsStalePreviewTimestamp::
                        kTimestampNotShownStalenessGreaterThanMax);
    return base::string16();
  }

  RecordStaleness(PreviewsStalePreviewTimestamp::kTimestampShown);

  if (staleness_in_minutes < 60) {
    return l10n_util::GetStringFUTF16(
        IDS_PREVIEWS_INFOBAR_TIMESTAMP_MINUTES,
        base::IntToString16(staleness_in_minutes));
  } else if (staleness_in_minutes < 120) {
    return l10n_util::GetStringUTF16(IDS_PREVIEWS_INFOBAR_TIMESTAMP_ONE_HOUR);
  } else {
    return l10n_util::GetStringFUTF16(
        IDS_PREVIEWS_INFOBAR_TIMESTAMP_HOURS,
        base::IntToString16(staleness_in_minutes / 60));
  }
}

void PreviewsUITabHelper::ReloadWithoutPreviews() {
  DCHECK(previews_user_data_);
  ReloadWithoutPreviews(previews_user_data_->committed_previews_type());
}

void PreviewsUITabHelper::ReloadWithoutPreviews(
    previews::PreviewsType previews_type) {
  InformPLMOfOptOut(web_contents());
#if defined(OS_ANDROID)
  should_display_android_omnibox_badge_ = false;
#endif
  if (on_dismiss_callback_)
    std::move(on_dismiss_callback_).Run(true);
  switch (previews_type) {
    case previews::PreviewsType::LITE_PAGE:
    case previews::PreviewsType::LITE_PAGE_REDIRECT:
    case previews::PreviewsType::OFFLINE:
    case previews::PreviewsType::NOSCRIPT:
    case previews::PreviewsType::RESOURCE_LOADING_HINTS:
      // Previews may cause a redirect, so we should use the original URL. The
      // black list prevents showing the preview again.
      web_contents()->GetController().Reload(
          content::ReloadType::ORIGINAL_REQUEST_URL, true);
      break;
    case previews::PreviewsType::LOFI:
      web_contents()->ReloadLoFiImages();
      break;
    case previews::PreviewsType::NONE:
    case previews::PreviewsType::UNSPECIFIED:
    case previews::PreviewsType::LAST:
    case previews::PreviewsType::DEPRECATED_AMP_REDIRECTION:
      NOTREACHED();
      break;
  }
}

void PreviewsUITabHelper::SetStalePreviewsStateForTesting(
    base::Time previews_freshness,
    bool is_reload) {
  previews_freshness_ = previews_freshness;
  is_stale_reload_ = is_reload;
}

void PreviewsUITabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Only show the ui if this is a full main frame navigation.
  if (!navigation_handle->IsInMainFrame() ||
      !navigation_handle->HasCommitted() || navigation_handle->IsSameDocument())
    return;

  previews_freshness_ = base::Time();
  previews_user_data_.reset();
#if defined(OS_ANDROID)
  should_display_android_omnibox_badge_ = false;
#endif
  previews::PreviewsUserData* user_data =
      GetPreviewsUserData(navigation_handle);

  // Store Previews information for this navigation.
  if (user_data) {
    previews_user_data_ =
        std::make_unique<previews::PreviewsUserData>(*user_data);
    // Delete this information later, so that other DidFinishNavigation methods
    // can reliably use GetPreviewsUserData regardless of order of
    // WebContentsObservers.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&PreviewsUITabHelper::RemovePreviewsUserData,
                       weak_factory_.GetWeakPtr(), navigation_handle));
  }

  uint64_t page_id = (previews_user_data_) ? previews_user_data_->page_id() : 0;

  // The ui should only be told if the page was a reload if the previous
  // page displayed a timestamp.
  is_stale_reload_ =
      displayed_preview_timestamp_
          ? navigation_handle->GetReloadType() != content::ReloadType::NONE
          : false;
  displayed_preview_ui_ = false;
  displayed_preview_timestamp_ = false;

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  offline_pages::OfflinePageTabHelper* tab_helper =
      offline_pages::OfflinePageTabHelper::FromWebContents(web_contents());

  if (tab_helper && tab_helper->GetOfflinePreviewItem()) {
    DCHECK_EQ(previews::PreviewsType::OFFLINE,
              previews_user_data_->committed_previews_type());
    if (navigation_handle->IsErrorPage()) {
      // TODO(ryansturm): Add UMA for errors.
      return;
    }
    data_reduction_proxy::DataReductionProxySettings*
        data_reduction_proxy_settings =
            DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
                web_contents()->GetBrowserContext());

    const offline_pages::OfflinePageItem* offline_page =
        tab_helper->GetOfflinePreviewItem();
    // From UMA, the median percent of network body bytes loaded out of total
    // body bytes on a page load. See PageLoad.Experimental.Bytes.Network and
    // PageLoad.Experimental.Bytes.Total.
    int64_t uncached_size = offline_page->file_size * 0.55;

    bool data_saver_enabled =
        data_reduction_proxy_settings->IsDataReductionProxyEnabled();

    data_reduction_proxy_settings->data_reduction_proxy_service()
        ->UpdateDataUseForHost(0, uncached_size,
                               navigation_handle->GetRedirectChain()[0].host());

    data_reduction_proxy_settings->data_reduction_proxy_service()
        ->UpdateContentLengths(0, uncached_size, data_saver_enabled,
                               data_reduction_proxy::HTTPS, "multipart/related",
                               true,
                               data_use_measurement::DataUseUserData::OTHER, 0);

    ShowUIElement(previews::PreviewsType::OFFLINE,
                  data_reduction_proxy_settings && data_saver_enabled,
                  base::BindOnce(&AddPreviewNavigationCallback,
                                 web_contents()->GetBrowserContext(),
                                 navigation_handle->GetRedirectChain()[0],
                                 previews::PreviewsType::OFFLINE, page_id));

    // Don't try to show other UIs if this is an offline preview.
    return;
  }
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)

  // Check for committed main frame preview.
  if (previews_user_data_ && previews_user_data_->HasCommittedPreviewsType()) {
    previews::PreviewsType main_frame_preview =
        previews_user_data_->committed_previews_type();
    if (ShouldShowUIForPreviewsType(main_frame_preview)) {
      if (main_frame_preview == previews::PreviewsType::LITE_PAGE) {
        const net::HttpResponseHeaders* headers =
            navigation_handle->GetResponseHeaders();
        if (headers)
          headers->GetDateValue(&previews_freshness_);
      }

      ShowUIElement(main_frame_preview, true /* is_data_saver_user */,
                    base::BindOnce(&AddPreviewNavigationCallback,
                                   web_contents()->GetBrowserContext(),
                                   navigation_handle->GetRedirectChain()[0],
                                   main_frame_preview, page_id));
    }
  }
}

previews::PreviewsUserData*
PreviewsUITabHelper::CreatePreviewsUserDataForNavigationHandle(
    content::NavigationHandle* navigation_handle,
    int64_t page_id) {
  inflight_previews_user_datas_.emplace(
      std::piecewise_construct, std::forward_as_tuple(navigation_handle),
      std::forward_as_tuple(page_id));

  auto data = inflight_previews_user_datas_.find(navigation_handle);

  return data == inflight_previews_user_datas_.end() ? nullptr : &data->second;
}

previews::PreviewsUserData* PreviewsUITabHelper::GetPreviewsUserData(
    content::NavigationHandle* navigation_handle) {
  auto data = inflight_previews_user_datas_.find(navigation_handle);
  return data == inflight_previews_user_datas_.end() ? nullptr
                                                     : &(data->second);
}

void PreviewsUITabHelper::RemovePreviewsUserData(
    content::NavigationHandle* navigation_handle) {
  inflight_previews_user_datas_.erase(navigation_handle);
}

// static
const void* PreviewsUITabHelper::OptOutEventKey() {
  return &kOptOutEventKey;
}
