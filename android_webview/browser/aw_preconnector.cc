// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_preconnector.h"

#include <jni.h>

#include <memory>
#include <optional>

#include "android_webview/browser/aw_browser_context.h"
#include "base/metrics/histogram_functions.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/preconnect_manager.h"
#include "content/public/browser/preconnect_request.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/network_anonymization_key.h"
#include "net/socket/next_proto.h"
#include "services/network/public/cpp/constants.h"
#include "url/android/gurl_android.h"
#include "url/origin.h"
#include "url/url_constants.h"

// Has to come after all the FromJniType() / ToJniType() headers.
#include "android_webview/browser_jni_headers/AwPreconnector_jni.h"

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(AndroidWebViewPreconnectEvent)
enum class AwPreconnectEvent {
  kPreconnectCalled = 0,
  kSessionClosed = 1,
  kConnectionEstablishedQuic = 2,
  kConnectionEstablishedHttp2 = 3,
  kConnectionEstablishedOther = 4,
  kConnectionClosedWasUsedQuic = 5,
  kConnectionClosedWasUsedHttp2 = 6,
  kConnectionClosedWasUsedOther = 7,
  kConnectionClosedWasNotUsedQuic = 8,
  kConnectionClosedWasNotUsedHttp2 = 9,
  kConnectionClosedWasNotUsedOther = 10,
  kMaxValue = kConnectionClosedWasNotUsedOther,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:AndroidWebViewPreconnectEvent)

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

  mojo::PendingRemote<network::mojom::ConnectionChangeObserverClient> observer;
  PreconnectContext context{base::TimeTicks::Now(), url};
  receivers_.Add(this, observer.InitWithNewPipeAndPassReceiver(),
                 std::move(context));

  std::optional<net::ConnectionKeepAliveConfig> keepalive_config;

  bool was_blocked = !browser_context_->GetDefaultStoragePartition()
                          ->IsNetworkContextInitialized();
  base::ElapsedTimer timer;

  // Preconnection initiated at the Profile level is out of scope of connection
  // allowlists, so there is no associated frame/document context to restrict.
  // See https://wicg.github.io/connection-allowlists/#threat-model. Hence
  // pass a No-op network_restrictions_id.
  GetPreconnectManager().StartPreconnectUrl(
      url, /*allow_credentials=*/true, key, kWebViewPreconnectTrafficAnnotation,
      /*storage_partition_config=*/nullptr,
      network::GetNoOpNetworkRestrictionsId(), std::move(keepalive_config),
      std::move(observer));

  AwBrowserContext::RecordNetworkContextInitializationBlocking(
      "Preconnect", timer.Elapsed(), was_blocked);

  TRACE_EVENT1("android_webview", "Preconnect::Begin", "url", url);

  base::UmaHistogramEnumeration("Android.WebView.Preconnect.Event",
                                AwPreconnectEvent::kPreconnectCalled);

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

void AwPreconnector::OnConnectionEstablished(
    const net::ConnectionChangeNotifier::EstablishedConnectionInfo& info) {
  PreconnectContext& context = receivers_.current_context();
  context.connection_info = info.connection_info;

  if (info.connection_info == net::NextProto::kProtoQUIC) {
    base::UmaHistogramMediumTimes(
        "Android.WebView.Preconnect.SessionCreationTime.QUIC",
        info.connection_setup_time);
    base::UmaHistogramEnumeration(
        "Android.WebView.Preconnect.Event",
        AwPreconnectEvent::kConnectionEstablishedQuic);
  } else if (info.connection_info == net::NextProto::kProtoHTTP2) {
    base::UmaHistogramMediumTimes(
        "Android.WebView.Preconnect.SessionCreationTime.HTTP2",
        info.connection_setup_time);
    base::UmaHistogramEnumeration(
        "Android.WebView.Preconnect.Event",
        AwPreconnectEvent::kConnectionEstablishedHttp2);
  } else {
    base::UmaHistogramMediumTimes(
        "Android.WebView.Preconnect.SessionCreationTime.Other",
        info.connection_setup_time);
    base::UmaHistogramEnumeration(
        "Android.WebView.Preconnect.Event",
        AwPreconnectEvent::kConnectionEstablishedOther);
  }
}

void AwPreconnector::OnSessionClosed(bool was_ever_used_to_create_streams) {
  const PreconnectContext& context = receivers_.current_context();
  base::TimeDelta duration = base::TimeTicks::Now() - context.start_time;

  base::UmaHistogramMediumTimes("Android.WebView.Preconnect.ConnectionDuration",
                                duration);

  if (context.connection_info == net::NextProto::kProtoQUIC) {
    base::UmaHistogramMediumTimes(
        "Android.WebView.Preconnect.ConnectionDuration.QUIC", duration);
    base::UmaHistogramEnumeration(
        "Android.WebView.Preconnect.Event",
        was_ever_used_to_create_streams
            ? AwPreconnectEvent::kConnectionClosedWasUsedQuic
            : AwPreconnectEvent::kConnectionClosedWasNotUsedQuic);
  } else if (context.connection_info == net::NextProto::kProtoHTTP2) {
    base::UmaHistogramMediumTimes(
        "Android.WebView.Preconnect.ConnectionDuration.HTTP2", duration);
    base::UmaHistogramEnumeration(
        "Android.WebView.Preconnect.Event",
        was_ever_used_to_create_streams
            ? AwPreconnectEvent::kConnectionClosedWasUsedHttp2
            : AwPreconnectEvent::kConnectionClosedWasNotUsedHttp2);
  } else {
    base::UmaHistogramMediumTimes(
        "Android.WebView.Preconnect.ConnectionDuration.Other", duration);
    base::UmaHistogramEnumeration(
        "Android.WebView.Preconnect.Event",
        was_ever_used_to_create_streams
            ? AwPreconnectEvent::kConnectionClosedWasUsedOther
            : AwPreconnectEvent::kConnectionClosedWasNotUsedOther);
  }

  TRACE_EVENT2("android_webview", "Preconnect::OnSessionClosed", "url",
               context.url, "duration", duration);
}

void AwPreconnector::OnNetworkEvent(net::NetworkChangeEvent event) {}

void AwPreconnector::OnConnectionFailed() {}

}  // namespace android_webview

DEFINE_JNI(AwPreconnector)
