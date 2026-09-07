// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_ENTERPRISE_AUTHENTICATION_APP_LINK_MANAGER_H_
#define ANDROID_WEBVIEW_BROWSER_AW_ENTERPRISE_AUTHENTICATION_APP_LINK_MANAGER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/url_matcher/url_matcher.h"
#include "url/gurl.h"

class PrefRegistrySimple;

namespace android_webview {

namespace prefs {
// This pref contains a list of authentication urls, for which when webview is
// navigated to any of these urls, browse intent will be sent.
inline constexpr char kEnterpriseAuthAppLinkPolicy[] =
    "enterprise_auth_app_link_policy";
}  // namespace prefs

class EnterpriseAuthenticationAppLinkManager {
 public:
  explicit EnterpriseAuthenticationAppLinkManager(PrefService* pref_service);

  EnterpriseAuthenticationAppLinkManager(
      const EnterpriseAuthenticationAppLinkManager&) = delete;
  EnterpriseAuthenticationAppLinkManager& operator=(
      const EnterpriseAuthenticationAppLinkManager&) = delete;
  ~EnterpriseAuthenticationAppLinkManager();

  static void RegisterPrefs(PrefRegistrySimple* registry);

  bool IsEnterpriseAuthenticationUrl(const GURL& url);

 private:
  // Called when the policy value changes in Prefs.
  void OnPolicyUpdated();

  PrefChangeRegistrar pref_observer_;
  std::unique_ptr<url_matcher::URLMatcher> url_matcher_;
  raw_ptr<PrefService> pref_service_;
};
}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_ENTERPRISE_AUTHENTICATION_APP_LINK_MANAGER_H_
