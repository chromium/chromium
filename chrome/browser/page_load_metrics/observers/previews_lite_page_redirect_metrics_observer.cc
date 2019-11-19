// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/previews_lite_page_redirect_metrics_observer.h"

#include <memory>

#include "chrome/browser/previews/previews_ui_tab_helper.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/previews/content/previews_user_data.h"
#include "net/base/network_change_notifier.h"
#include "net/nqe/effective_connection_type.h"

PreviewsLitePageRedirectMetricsObserver::
    PreviewsLitePageRedirectMetricsObserver() = default;

PreviewsLitePageRedirectMetricsObserver::
    ~PreviewsLitePageRedirectMetricsObserver() = default;

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PreviewsLitePageRedirectMetricsObserver::OnCommitCalled(
    content::NavigationHandle* handle,
    ukm::SourceId source_id) {
  PreviewsUITabHelper* ui_tab_helper =
      PreviewsUITabHelper::FromWebContents(handle->GetWebContents());
  if (!ui_tab_helper)
    return STOP_OBSERVING;

  previews::PreviewsUserData* previews_data =
      ui_tab_helper->GetPreviewsUserData(handle);
  if (!previews_data) {
    return STOP_OBSERVING;
  }

  previews::PreviewsUserData::ServerLitePageInfo* info =
      previews_data->server_lite_page_info();
  if (!info || info->status == previews::ServerLitePageStatus::kUnknown)
    return STOP_OBSERVING;

  // Past this point, we know this navigation is a preview or at least attempted
  // one.

  // Populate our own DRP Data and push it up to the base class.
  std::unique_ptr<data_reduction_proxy::DataReductionProxyData> data =
      std::make_unique<data_reduction_proxy::DataReductionProxyData>();
  data->set_session_key(info->drp_session_key);
  data->set_page_id(info->page_id);
  data->set_effective_connection_type(previews_data->navigation_ect());
  data->set_lite_page_received(info->status ==
                               previews::ServerLitePageStatus::kSuccess);

  data->set_connection_type(net::NetworkChangeNotifier::GetConnectionType());
  data->set_request_url(handle->GetURL());
  data->set_black_listed(previews_data->black_listed_for_lite_page());
  data->set_used_data_reduction_proxy(true);
  data->set_was_cached_data_reduction_proxy_response(false);

  DCHECK_NE(info->status, previews::ServerLitePageStatus::kUnknown);

  set_data(std::move(data));
  set_lite_page_redirect_status(info->status);

  return CONTINUE_OBSERVING;
}
