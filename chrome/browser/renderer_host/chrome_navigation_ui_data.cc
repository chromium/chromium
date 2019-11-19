// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"

#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/prerender/prerender_histograms.h"
#include "chrome/browser/previews/previews_lite_page_redirect_decider.h"
#include "content/public/browser/navigation_handle.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/window_open_disposition.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/constants.h"
#endif

ChromeNavigationUIData::ChromeNavigationUIData()
    : disposition_(WindowOpenDisposition::CURRENT_TAB) {}

ChromeNavigationUIData::ChromeNavigationUIData(
    content::NavigationHandle* navigation_handle)
    : disposition_(WindowOpenDisposition::CURRENT_TAB) {
  auto* web_contents = navigation_handle->GetWebContents();
#if BUILDFLAG(ENABLE_EXTENSIONS)
  int tab_id = extension_misc::kUnknownTabId;
  int window_id = extension_misc::kUnknownWindowId;
  // The browser client can be null in unittests.
  if (extensions::ExtensionsBrowserClient::Get()) {
    extensions::ExtensionsBrowserClient::Get()->GetTabAndWindowIdForWebContents(
        web_contents, &tab_id, &window_id);
  }
  extension_data_ = std::make_unique<extensions::ExtensionNavigationUIData>(
      navigation_handle, tab_id, window_id);
#endif

  auto* prerender_contents =
      prerender::PrerenderContents::FromWebContents(web_contents);
  if (prerender_contents) {
    prerender_mode_ = prerender_contents->prerender_mode();
    prerender_histogram_prefix_ =
        prerender::PrerenderHistograms::GetHistogramPrefix(
            prerender_contents->origin());
  }
  data_reduction_proxy_page_id_ =
      PreviewsLitePageRedirectDecider::GeneratePageIdForWebContents(
          web_contents);
}

ChromeNavigationUIData::~ChromeNavigationUIData() {}

// static
std::unique_ptr<ChromeNavigationUIData>
ChromeNavigationUIData::CreateForMainFrameNavigation(
    content::WebContents* web_contents,
    WindowOpenDisposition disposition,
    int64_t data_reduction_proxy_page_id) {
  auto navigation_ui_data = std::make_unique<ChromeNavigationUIData>();
  navigation_ui_data->disposition_ = disposition;
  navigation_ui_data->data_reduction_proxy_page_id_ =
      data_reduction_proxy_page_id;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  int tab_id = extension_misc::kUnknownTabId;
  int window_id = extension_misc::kUnknownWindowId;
  // The browser client can be null in unittests.
  if (extensions::ExtensionsBrowserClient::Get()) {
    extensions::ExtensionsBrowserClient::Get()->GetTabAndWindowIdForWebContents(
        web_contents, &tab_id, &window_id);
  }

  navigation_ui_data->extension_data_ =
      extensions::ExtensionNavigationUIData::CreateForMainFrameNavigation(
          web_contents, tab_id, window_id);
#endif

  return navigation_ui_data;
}

std::unique_ptr<content::NavigationUIData> ChromeNavigationUIData::Clone() {
  auto copy = std::make_unique<ChromeNavigationUIData>();

  copy->disposition_ = disposition_;
  copy->data_reduction_proxy_page_id_ = data_reduction_proxy_page_id_;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (extension_data_)
    copy->SetExtensionNavigationUIData(extension_data_->DeepCopy());
#endif

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  if (offline_page_data_)
    copy->SetOfflinePageNavigationUIData(offline_page_data_->DeepCopy());
#endif

  copy->prerender_mode_ = prerender_mode_;
  copy->prerender_histogram_prefix_ = prerender_histogram_prefix_;

  return std::move(copy);
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
void ChromeNavigationUIData::SetExtensionNavigationUIData(
    std::unique_ptr<extensions::ExtensionNavigationUIData> extension_data) {
  extension_data_ = std::move(extension_data);
}
#endif

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
void ChromeNavigationUIData::SetOfflinePageNavigationUIData(
    std::unique_ptr<offline_pages::OfflinePageNavigationUIData>
        offline_page_data) {
  offline_page_data_ = std::move(offline_page_data);
}
#endif
