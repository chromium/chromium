// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_preconnector.h"

#include <jni.h>

#include <memory>
#include <optional>

#include "base/trace_event/trace_event.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/preconnect_manager.h"
#include "content/public/browser/preconnect_request.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/network_anonymization_key.h"
#include "url/android/gurl_android.h"
#include "url/origin.h"
#include "url/url_constants.h"

// Has to come after all the FromJniType() / ToJniType() headers.
#include "android_webview/browser_jni_headers/AwPreconnector_jni.h"

namespace {

inline constexpr net::NetworkTrafficAnnotationTag
    kWebViewPreconnectTrafficAnnotation =
        net::DefineNetworkTrafficAnnotation("webview_preconnect",
                                            R"(
        semantics {
          sender: "Android WebView"
          description: "WebView is an Android component that allows Android "
            "applications to render web contents in their app. The WebView "
            "preconnect API allows apps to open a connection to a domain "
            "before loading any pages to speed up future loads."
          trigger: "This is triggered when an application uses WebView's "
            "Profile#preconnect API. It is up to the Android developer to "
            "decide when to call this."
          internal {
            contacts {
              owners: "//android_webview/OWNERS"
            }
          }
          user_data {
            type: ACCESS_TOKEN
          }
          data: "None"
          destination: WEBSITE
          last_reviewed: "2025-08-01"
        }
        policy {
          cookies_allowed: NO
          setting: "Not user controlled"
          policy_exception_justification:
            "No data is sent beyond what is included in a normal page load "
            "triggered by WebView#loadUrl. This API is purely an optimization "
            ", allowing the opening of the network request to be moved "
            "earlier."
        }
      )");

}  // anonymous namespace

namespace android_webview {

AwPreconnector::AwPreconnector(content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}

AwPreconnector::~AwPreconnector() {
  if (java_obj_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_AwPreconnector_destroy(env, java_obj_);
  }
}

bool AwPreconnector::Preconnect(JNIEnv* env, const GURL& url) {
  // Network anonymization isn't implemented for WebView, so we can use an empty
  // key.
  net::NetworkAnonymizationKey key = net::NetworkAnonymizationKey();

  if (!url.is_valid()) {
    return false;
  }

  url::Origin origin = url::Origin::Create(url);
  if ((origin.scheme() != url::kHttpScheme) &&
      (origin.scheme() != url::kHttpsScheme)) {
    // Cannot preconnect to local or opaque origins.
    return false;
  }

  std::vector<content::PreconnectRequest> requests = {
      content::PreconnectRequest(origin, /* num_sockets= */ 1, key)};
  GetPreconnectManager().Start(url, requests,
                               kWebViewPreconnectTrafficAnnotation);
  TRACE_EVENT1("android_webview", "Preconnect::Begin", "url", url);

  return true;
}

void AwPreconnector::PreconnectInitiated(const GURL& url,
                                         const GURL& preconnect_url) {}

void AwPreconnector::PreconnectFinished(
    std::unique_ptr<content::PreconnectStats> stats) {
  TRACE_EVENT1("android_webview", "Preconnect::Finished", "url", stats->url);
}

bool AwPreconnector::IsPreconnectEnabled() {
  return true;
}

base::android::ScopedJavaLocalRef<jobject>
AwPreconnector::GetJavaAwPreconnector() {
  if (!java_obj_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    java_obj_ =
        Java_AwPreconnector_create(env, reinterpret_cast<intptr_t>(this));
  }

  return base::android::ScopedJavaLocalRef<jobject>(java_obj_);
}

content::PreconnectManager& AwPreconnector::GetPreconnectManager() {
  if (!preconnect_manager_) {
    preconnect_manager_ = content::PreconnectManager::Create(
        weak_factory_.GetWeakPtr(), browser_context_);
  }

  return *preconnect_manager_.get();
}

}  // namespace android_webview

DEFINE_JNI(AwPreconnector)
