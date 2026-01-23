// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/page_load_metrics/aw_gws_page_load_metrics_observer.h"

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/cookie_manager.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/parsed_cookie.h"
#include "url/gurl.h"

namespace android_webview {

namespace {
constexpr char kGwsOriginUrl[] = "https://www.google.com/";
constexpr char kWarmaUpCookieName[] = "SSID";
}  // namespace

AwGWSPageLoadMetricsObserver::AwGWSPageLoadMetricsObserver() = default;

bool AwGWSPageLoadMetricsObserver::IsFromNewTabPage(
    content::NavigationHandle* navigation_handle) {
  return false;
}

bool AwGWSPageLoadMetricsObserver::IsBrowserStartupComplete() {
  return true;
}

bool AwGWSPageLoadMetricsObserver::IsIncognitoProfile() const {
  // Always returns false since WebView does not have Incognito mode.
  return false;
}

bool AwGWSPageLoadMetricsObserver::IsSignedIn(
    content::BrowserContext* browser_context) const {
  AwBrowserContext* aw_browser_context =
      static_cast<AwBrowserContext*>(browser_context);
  CHECK(aw_browser_context);
  CookieManager* cookie_manager = aw_browser_context->GetCookieManager();
  CHECK(cookie_manager);

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> url =
      base::android::ConvertUTF8ToJavaString(env, kGwsOriginUrl);
  std::string cookies = cookie_manager->GetCookie(env, url);
  net::cookie_util::ParsedRequestCookies parsed_cookies;
  net::cookie_util::ParseRequestCookieLine(cookies, &parsed_cookies);
  for (const auto& pair : parsed_cookies) {
    if (pair.first == kWarmaUpCookieName) {
      return true;
    }
  }

  return false;
}

content::BrowserContext*
AwGWSPageLoadMetricsObserver::GetOriginalBrowserContext() {
  return nullptr;
}

}  // namespace android_webview
