// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"

#include "chrome/browser/preloading/prefetch/no_state_prefetch/chrome_no_state_prefetch_contents_delegate.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_contents.h"
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

  auto* no_state_prefetch_contents =
      prerender::ChromeNoStatePrefetchContentsDelegate::FromWebContents(
          web_contents);
  if (no_state_prefetch_contents) {
    is_no_state_prefetching_ = true;
  }
}

ChromeNavigationUIData::~ChromeNavigationUIData() {}

// static
std::unique_ptr<ChromeNavigationUIData>
ChromeNavigationUIData::CreateForMainFrameNavigation(
    content::WebContents* web_contents,
    WindowOpenDisposition disposition,
    bool is_using_https_as_default_scheme,
    bool force_no_https_upgrade) {
  auto navigation_ui_data = std::make_unique<ChromeNavigationUIData>();
  navigation_ui_data->disposition_ = disposition;
  navigation_ui_data->is_using_https_as_default_scheme_ =
      is_using_https_as_default_scheme;
  navigation_ui_data->force_no_https_upgrade_ = force_no_https_upgrade;

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
  copy->is_using_https_as_default_scheme_ = is_using_https_as_default_scheme_;
  copy->force_no_https_upgrade_ = force_no_https_upgrade_;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (extension_data_)
    copy->SetExtensionNavigationUIData(extension_data_->DeepCopy());
#endif

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  if (offline_page_data_)
    copy->SetOfflinePageNavigationUIData(offline_page_data_->DeepCopy());
#endif

  copy->is_no_state_prefetching_ = is_no_state_prefetching_;
  copy->bookmark_id_ = bookmark_id_;

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
