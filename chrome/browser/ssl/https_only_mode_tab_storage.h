// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_HTTPS_ONLY_MODE_TAB_STORAGE_H_
#define CHROME_BROWSER_SSL_HTTPS_ONLY_MODE_TAB_STORAGE_H_

#include <set>
#include <string>

#include "base/supports_user_data.h"

namespace content {
class WebContents;
}  // namespace content

// A short-lived, per-tab storage for the HTTPS-Only Mode allowlist and data
// about the navigation.
class HttpsOnlyModeTabStorage : public base::SupportsUserData::Data {
 public:
  HttpsOnlyModeTabStorage();
  ~HttpsOnlyModeTabStorage() override;

  HttpsOnlyModeTabStorage(const HttpsOnlyModeTabStorage&) = delete;
  HttpsOnlyModeTabStorage& operator=(const HttpsOnlyModeTabStorage&) = delete;

  static HttpsOnlyModeTabStorage* GetOrCreate(
      content::WebContents* web_contents);

  void AddHostToAllowlist(const std::string& hostname);
  bool IsHostAllowlisted(const std::string& hostname);

  void set_is_navigation_upgraded(bool upgraded) {
    is_navigation_upgraded_ = upgraded;
  }
  bool is_navigation_upgraded() const { return is_navigation_upgraded_; }

 private:
  // TODO(crbug.com/1218526): Track upgrade status per-navigation rather than
  // per-WebContents, in case multiple navigations occur in the WebContents and
  // the metadata is not cleared. This may be tricky however as the Interceptor
  // and the Throttle have slightly different views of the navigation -- the
  // Throttle has a NavigationHandle (and thus the Navigation ID) but the
  // Interceptor has the NavigationEntry's ID which does not match.
  bool is_navigation_upgraded_ = false;

  // A simple tab-scoped allowlist of hostnames.
  // TODO(crbug.com/1218526): Replace with persistent allowlist implementation.
  std::set<std::string> allowlist_;
};

#endif  // CHROME_BROWSER_SSL_HTTPS_ONLY_MODE_TAB_STORAGE_H_
