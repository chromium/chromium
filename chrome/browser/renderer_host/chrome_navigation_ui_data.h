// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_CHROME_NAVIGATION_UI_DATA_H_
#define CHROME_BROWSER_RENDERER_HOST_CHROME_NAVIGATION_UI_DATA_H_

#include <memory>
#include <optional>
#include <string>

#include "build/build_config.h"
#include "components/actor/core/task_id.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "components/offline_pages/core/request_header/offline_page_navigation_ui_data.h"
#include "content/public/browser/navigation_ui_data.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "extensions/browser/extension_navigation_ui_data.h"
#endif

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

// Carries `//chrome` navigation metadata across navigation params,
// `NavigationHandle`, and `NavigationURLLoader` on the UI thread.
//
// Lifecycle events:
//
// - Creation: Either created upfront by the caller before the navigation
//   starts (e.g. main-frame navigations via `Navigate()`), or created by
//   `//content` from the `NavigationHandle` right before the request starts if
//   no upfront instance was supplied. In the latter case, fields set by the
//   caller keep their default values.
// - Same-navigation copy: A copy is made for use within the same navigation
//   (e.g. for the `NavigationURLLoader`), taking the field values at that
//   point.
// - Loader update: The loader updates its copy while the request is in
//   flight.
// - Re-dispatch: A throttle restarts the navigation as a new one derived from
//   the `NavigationHandle` (e.g. an HTTP fallback), possibly to a different
//   URL.
//
// Each field documents its behavior on these events. Events that a field does
// not mention leave it unchanged.
class ChromeNavigationUIData : public content::NavigationUIData {
 public:
  ChromeNavigationUIData();
  explicit ChromeNavigationUIData(content::NavigationHandle* navigation_handle);

  ChromeNavigationUIData(const ChromeNavigationUIData&) = delete;
  ChromeNavigationUIData& operator=(const ChromeNavigationUIData&) = delete;

  ~ChromeNavigationUIData() override;

  // Creates an instance of ChromeNavigationUIData associated with the given
  // |web_contents|.
  // If |is_using_https_as_default_scheme|, this is a typed main frame
  // navigation where the omnibox used HTTPS as the default URL scheme because
  // the user didn't type a scheme (e.g. they entered "example.com" and we
  // are navigating to https://example.com).
  static std::unique_ptr<ChromeNavigationUIData> CreateForMainFrameNavigation(
      content::WebContents* web_contents,
      bool is_using_https_as_default_scheme,
      bool force_no_https_upgrade);

  // Creates a new ChromeNavigationUIData that is a deep copy of the original.
  // Any changes to the original after the clone is created will not be
  // reflected in the clone.  All owned data members are deep copied.
  std::unique_ptr<content::NavigationUIData> Clone() override;

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
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
  bool is_no_state_prefetching() const { return is_no_state_prefetching_; }
  bool is_using_https_as_default_scheme() const {
    return is_using_https_as_default_scheme_;
  }
  bool force_no_https_upgrade() const { return force_no_https_upgrade_; }

  std::optional<int64_t> bookmark_id() { return bookmark_id_; }
  void set_bookmark_id(std::optional<int64_t> id) { bookmark_id_ = id; }

#if BUILDFLAG(IS_ANDROID)
  // The token identifying the TWA launch associated with this navigation,
  // used to match JNI launches with C++ navigations.
  std::optional<int64_t> twa_launch_token() const { return twa_launch_token_; }
  void set_twa_launch_token(std::optional<int64_t> token) {
    twa_launch_token_ = token;
  }
#endif

  actor::TaskId actor_task_id() const { return actor_task_id_; }

  bool navigation_initiated_from_sync() {
    return navigation_initiated_from_sync_;
  }
  void set_navigation_initiated_from_sync(bool navigation_initiated_from_sync) {
    navigation_initiated_from_sync_ = navigation_initiated_from_sync;
  }

 private:
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  // Lifecycle:
  //
  // - Creation: Snapshotted from the `WebContents` (via
  //   `CreateForMainFrameNavigation()`) or the `NavigationHandle` (created by
  //   `//content`); left unset for direct `ChromeNavigationUIData()` callers.
  // - Same-navigation copy: Copied.
  // - Re-dispatch: Dropped.
  std::unique_ptr<extensions::ExtensionNavigationUIData> extension_data_;
#endif

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  // Lifecycle:
  //
  // - Loader update: Set by `OfflinePageURLLoader` on the loader's copy.
  // - Same-navigation copy: Copied.
  // - Re-dispatch: Dropped.
  std::unique_ptr<offline_pages::OfflinePageNavigationUIData>
      offline_page_data_;
#endif

  // Lifecycle:
  //
  // - Creation: Snapshotted from the `WebContents` when created by
  //   `//content`; left false for upfront instances.
  // - Same-navigation copy: Copied.
  // - Re-dispatch: Dropped.
  bool is_no_state_prefetching_ = false;

  // True if the navigation was initiated by typing in the omnibox but the typed
  // text didn't have a scheme such as http or https (e.g. google.com), and
  // https was used as the default scheme for the navigation. This is used by
  // TypedNavigationUpgradeThrottle to determine if the navigation should be
  // observed and fall back to using http scheme if necessary.
  //
  // Lifecycle:
  //
  // - Creation: Set by the caller.
  // - Same-navigation copy: Copied.
  // - Re-dispatch: Dropped.
  bool is_using_https_as_default_scheme_ = false;

  // True if the navigation should be excluded from HTTPS upgrades.
  // This can happen in the following cases:
  //
  // - the navigation was initiated by typing in the omnibox, and the
  //   typed text had an explicit http scheme.
  // - the navigation was initiated as a captive portal login.
  //
  // Lifecycle:
  //
  // - Creation: Set by the caller.
  // - Same-navigation copy: Copied.
  // - Re-dispatch: Dropped.
  bool force_no_https_upgrade_ = false;

  // Id of the bookmark which started this navigation.
  //
  // Lifecycle:
  //
  // - Creation: Set by the caller on Android.
  // - Same-navigation copy: Copied.
  // - Re-dispatch: Dropped.
  //
  // Exception: On desktop, bookmark UIs set this on the `NavigationHandle`'s
  // instance after `Navigate()` returns, i.e. after the navigation has
  // started, because `Navigate()` cannot take upfront `NavigationUIData`. It
  // works only because `HistoryTabHelper` reads it from the
  // `NavigationHandle`'s instance later in the navigation; it is unavailable
  // at `DidStartNavigation()` and may be missing from copies made earlier.
  // TODO(https://crbug.com/517725655): Set it upfront at creation instead.
  std::optional<int64_t> bookmark_id_;

#if BUILDFLAG(IS_ANDROID)
  // Unique token generated in Java to identify a TWA launch associated with
  // this navigation.
  //
  // Lifecycle:
  //
  // - Creation: Set by the caller.
  // - Same-navigation copy: Copied.
  // - Re-dispatch: Dropped.
  std::optional<int64_t> twa_launch_token_;
#endif

  // True if the navigation was initiated in response to a sync message. This is
  // used in tab group sync to identify the sync initiated navigations and
  // blocking them from sending back to sync which would otherwise cause a
  // ping-pong issue. They will still be allowed to load locally like a normal
  // navigation.
  //
  // Lifecycle:
  //
  // - Creation: Set by the caller.
  // - Same-navigation copy: Copied.
  // - Re-dispatch: Dropped.
  bool navigation_initiated_from_sync_ = false;

  // Id of the actor task active during this navigation. Set only if actor was
  // acting on the tab when the navigation started.
  //
  // Lifecycle:
  //
  // - Creation: Snapshotted from the `WebContents` (via
  //   `CreateForMainFrameNavigation()` or when created by `//content`); left
  //   unset for direct `ChromeNavigationUIData()` callers.
  // - Same-navigation copy: Copied.
  // - Re-dispatch: Dropped.
  actor::TaskId actor_task_id_;
};

#endif  // CHROME_BROWSER_RENDERER_HOST_CHROME_NAVIGATION_UI_DATA_H_
