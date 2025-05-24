// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/network_service/aw_web_resource_request.h"

#include "android_webview/browser/network_service/net_helpers.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "ui/base/page_transition_types.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/AwWebResourceRequest_jni.h"

namespace android_webview {

AwWebResourceRequest::AwWebResourceRequest(
    const network::ResourceRequest& request)
    : url(request.url.spec()),
      method(request.method),
      is_outermost_main_frame(request.destination ==
                              network::mojom::RequestDestination::kDocument),
      has_user_gesture(request.has_user_gesture),
      is_renderer_initiated(ui::PageTransitionIsWebTriggerable(
          static_cast<ui::PageTransition>(request.transition_type))) {
  ConvertRequestHeadersToVectors(request.headers, &header_names,
                                 &header_values);
}

AwWebResourceRequest::AwWebResourceRequest(
    const std::string& in_url,
    const std::string& in_method,
    bool in_is_outermost_main_frame,
    bool in_has_user_gesture,
    const net::HttpRequestHeaders& in_headers)
    : url(in_url),
      method(in_method),
      is_outermost_main_frame(in_is_outermost_main_frame),
      has_user_gesture(in_has_user_gesture) {
  ConvertRequestHeadersToVectors(in_headers, &header_names, &header_values);
}

AwWebResourceRequest::AwWebResourceRequest(const AwWebResourceRequest& other) =
    default;
AwWebResourceRequest::AwWebResourceRequest(AwWebResourceRequest&& other) =
    default;
AwWebResourceRequest& AwWebResourceRequest::operator=(
    AwWebResourceRequest&& other) = default;
AwWebResourceRequest::~AwWebResourceRequest() = default;

}  // namespace android_webview
//
namespace jni_zero {
template <>
ScopedJavaLocalRef<jobject> ToJniType(
    JNIEnv* env,
    const android_webview::AwWebResourceRequest& request) {
  return android_webview::Java_AwWebResourceRequest_create(
      env, request.url, request.is_outermost_main_frame,
      request.has_user_gesture, request.method, request.header_names,
      request.header_values);
}
}  // namespace jni_zero
