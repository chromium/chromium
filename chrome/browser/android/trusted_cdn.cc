// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/trusted_cdn.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/android/chrome_jni_headers/TrustedCdn_jni.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/offline_pages/offline_page_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using content::WebContents;

constexpr char kDefaultTrustedCDNBaseURL[] = "https://cdn.ampproject.org";

namespace {
// Returns whether the given URL is hosted by a trusted CDN. This can be turned
// off via a Feature, and the base URL to trust can be set via a command line
// flag for testing.
bool IsTrustedCDN(const GURL& url) {
  if (!base::FeatureList::IsEnabled(features::kShowTrustedPublisherURL))
    return false;

  // Use a static local (without destructor) to construct the base URL only
  // once. |trusted_cdn_base_url| is initialized with the result of an
  // immediately evaluated lambda, which allows wrapping the code in a single
  // expression.
  static const base::NoDestructor<GURL> trusted_cdn_base_url([]() {
    const base::CommandLine* command_line =
        base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(switches::kTrustedCDNBaseURLForTests)) {
      GURL base_url(command_line->GetSwitchValueASCII(
          switches::kTrustedCDNBaseURLForTests));
      LOG_IF(WARNING, !base_url.is_valid()) << "Invalid trusted CDN base URL: "
                                            << base_url.possibly_invalid_spec();
      return base_url;
    }

    return GURL(kDefaultTrustedCDNBaseURL);
  }());

  // Allow any subdomain of the base URL.
  return url.DomainIs(trusted_cdn_base_url->host_piece()) &&
         (url.scheme_piece() == trusted_cdn_base_url->scheme_piece()) &&
         (url.EffectiveIntPort() == trusted_cdn_base_url->EffectiveIntPort());
}

GURL GetPublisherURL(content::NavigationHandle* navigation_handle) {
  if (!IsTrustedCDN(navigation_handle->GetURL()))
    return GURL();

  // Offline pages don't have headers when they are loaded.
  // TODO(bauerb): Consider storing the publisher URL on the offline page item.
  if (offline_pages::OfflinePageUtils::GetOfflinePageFromWebContents(
          navigation_handle->GetWebContents())) {
    return GURL();
  }

  const net::HttpResponseHeaders* headers =
      navigation_handle->GetResponseHeaders();
  if (!headers) {
    // TODO(https://crbug.com/829323): In some cases other than offline pages
    // we don't have headers.
    LOG(WARNING) << "No headers for navigation to "
                 << navigation_handle->GetURL();
    return GURL();
  }

  std::string publisher_url;
  if (!headers->GetNormalizedHeader("x-amp-cache", &publisher_url))
    return GURL();

  return GURL(publisher_url);
}

}  // namespace

TrustedCdn::TrustedCdn(JNIEnv* env, const JavaParamRef<jobject>& obj)
    : jobj_(env, obj) {}

TrustedCdn::~TrustedCdn() = default;

void TrustedCdn::SetWebContents(JNIEnv* env,
                                const JavaParamRef<jobject>& obj,
                                const JavaParamRef<jobject>& jweb_contents) {
  WebContentsObserver::Observe(WebContents::FromJavaWebContents(jweb_contents));
}

void TrustedCdn::ResetWebContents(JNIEnv* env,
                                  const JavaParamRef<jobject>& obj) {
  WebContentsObserver::Observe(nullptr);
}

void TrustedCdn::OnDestroyed(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  delete this;
}

void TrustedCdn::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Skip subframe, same-document, or non-committed navigations (downloads or
  // 204/205 responses).
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  GURL publisher_url = GetPublisherURL(navigation_handle);
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> j_publisher_url;
  if (publisher_url.is_valid())
    j_publisher_url = ConvertUTF8ToJavaString(env, publisher_url.spec());

  Java_TrustedCdn_setPublisherUrl(env, jobj_, j_publisher_url);
}

static jlong JNI_TrustedCdn_Init(JNIEnv* env,
                                 const JavaParamRef<jobject>& obj) {
  return reinterpret_cast<intptr_t>(new TrustedCdn(env, obj));
}
