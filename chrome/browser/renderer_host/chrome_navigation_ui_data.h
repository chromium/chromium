// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_CHROME_NAVIGATION_UI_DATA_H_
#define CHROME_BROWSER_RENDERER_HOST_CHROME_NAVIGATION_UI_DATA_H_

#include <memory>
#include <optional>
#include <string>

#include "components/offline_pages/buildflags/buildflags.h"
#include "components/offline_pages/core/request_header/offline_page_navigation_ui_data.h"
#include "content/public/browser/navigation_ui_data.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_navigation_ui_data.h"
#endif

namespace content {
class NavigationHandle;
class WebContents;
}

enum class WindowOpenDisposition;

// Contains data that is passed from the UI thread to the IO thread at the
// beginning of each navigation. The class is instantiated on the UI thread,
// then a copy created using Clone is passed to the content::ResourceRequestInfo
// on the IO thread.
class ChromeNavigationUIData : public content::NavigationUIData {
 public:
  ChromeNavigationUIData();
  explicit ChromeNavigationUIData(content::NavigationHandle* navigation_handle);

  ChromeNavigationUIData(const ChromeNavigationUIData&) = delete;
  ChromeNavigationUIData& operator=(const ChromeNavigationUIData&) = delete;

  ~ChromeNavigationUIData() override;

  // Creates an instance of ChromeNavigationUIData associated with the given
  // |web_contents| with the given |disposition|.
  // If |is_using_https_as_default_scheme|, this is a typed main frame
  // navigation where the omnibox used HTTPS as the default URL scheme because
  // the user didn't type a scheme (e.g. they entered "example.com" and we
  // are navigating to https://example.com).
  static std::unique_ptr<ChromeNavigationUIData> CreateForMainFrameNavigation(
      content::WebContents* web_contents,
      WindowOpenDisposition disposition,
      bool is_using_https_as_default_scheme,
      bool force_no_https_upgrade);

  // Creates a new ChromeNavigationUIData that is a deep copy of the original.
  // Any changes to the original after the clone is created will not be
  // reflected in the clone.  All owned data members are deep copied.
  std::unique_ptr<content::NavigationUIData> Clone() override;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  void SetExtensionNavigationUIData(
      std::unique_ptr<extensions::ExtensionNavigationUIData> extension_data);

  extensions::ExtensionNavigationUIData* GetExtensionNavigationUIData() const {
    return extension_data_.get();
  }
#endif

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  void SetOfflinePageNavigationUIData(
      std::unique_ptr<offline_pages::OfflinePageNavigationUIData>
          offline_page_data);

  offline_pages::OfflinePageNavigationUIData* GetOfflinePageNavigationUIData()
      const {
    return offline_page_data_.get();
  }
#endif
  WindowOpenDisposition window_open_disposition() const { return disposition_; }
  bool is_no_state_prefetching() const { return is_no_state_prefetching_; }
  bool is_using_https_as_default_scheme() const {
    return is_using_https_as_default_scheme_;
  }
  bool force_no_https_upgrade() const { return force_no_https_upgrade_; }

  std::optional<int64_t> bookmark_id() { return bookmark_id_; }
  void set_bookmark_id(std::optional<int64_t> id) { bookmark_id_ = id; }

 private:
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Manages the lifetime of optional ExtensionNavigationUIData information.
  std::unique_ptr<extensions::ExtensionNavigationUIData> extension_data_;
#endif

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  // Manages the lifetime of optional OfflinePageNavigationUIData information.
  std::unique_ptr<offline_pages::OfflinePageNavigationUIData>
      offline_page_data_;
#endif

  WindowOpenDisposition disposition_;
  bool is_no_state_prefetching_ = false;

  // True if the navigation was initiated by typing in the omnibox but the typed
  // text didn't have a scheme such as http or https (e.g. google.com), and
  // https was used as the default scheme for the navigation. This is used by
  // TypedNavigationUpgradeThrottle to determine if the navigation should be
  // observed and fall back to using http scheme if necessary.
  bool is_using_https_as_default_scheme_ = false;

  // True if the navigation should be excluded from HTTPS upgrades.
  // This can happen in the following cases:
  // - the navigatioon was initiated by typing in the omnibox, and the
  // typed text had an explicit http scheme.
  // - the navigation was initiated as a captive portal login.
  bool force_no_https_upgrade_ = false;

  // Id of the bookmark which started this navigation.
  std::optional<int64_t> bookmark_id_ = std::nullopt;
};

#endif  // CHROME_BROWSER_RENDERER_HOST_CHROME_NAVIGATION_UI_DATA_H_
