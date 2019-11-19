// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/network_service/aw_web_resource_response.h"

#include <memory>
#include <string>
#include <vector>

#include "android_webview/browser/input_stream.h"
#include "android_webview/browser_jni_headers/AwWebResourceResponse_jni.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"

using base::android::ScopedJavaLocalRef;
using base::android::AppendJavaStringArrayToStringVector;

namespace android_webview {

AwWebResourceResponse::AwWebResourceResponse(
    const base::android::JavaRef<jobject>& obj)
    : java_object_(obj), input_stream_transferred_(false) {}

AwWebResourceResponse::~AwWebResourceResponse() {}

bool AwWebResourceResponse::HasInputStream(JNIEnv* env) const {
  ScopedJavaLocalRef<jobject> jstream =
      Java_AwWebResourceResponse_getData(env, java_object_);
  return !jstream.is_null();
}

std::unique_ptr<InputStream> AwWebResourceResponse::GetInputStream(
    JNIEnv* env) {
  // Only allow to call GetInputStream once per object, because this method
  // transfers ownership of the stream and once the unique_ptr<InputStream>
  // is deleted it also closes the original java input stream. This
  // side-effect can result in unexpected behavior, e.g. trying to read
  // from a closed stream.
  DCHECK(!input_stream_transferred_);

  if (input_stream_transferred_)
    return nullptr;

  input_stream_transferred_ = true;
  ScopedJavaLocalRef<jobject> jstream =
      Java_AwWebResourceResponse_getData(env, java_object_);
  if (jstream.is_null())
    return nullptr;
  return std::make_unique<InputStream>(jstream);
}

bool AwWebResourceResponse::GetMimeType(JNIEnv* env,
                                        std::string* mime_type) const {
  ScopedJavaLocalRef<jstring> jstring_mime_type =
      Java_AwWebResourceResponse_getMimeType(env, java_object_);
  if (jstring_mime_type.is_null())
    return false;
  *mime_type = ConvertJavaStringToUTF8(jstring_mime_type);
  return true;
}

bool AwWebResourceResponse::GetCharset(JNIEnv* env,
                                       std::string* charset) const {
  ScopedJavaLocalRef<jstring> jstring_charset =
      Java_AwWebResourceResponse_getCharset(env, java_object_);
  if (jstring_charset.is_null())
    return false;
  *charset = ConvertJavaStringToUTF8(jstring_charset);
  return true;
}

bool AwWebResourceResponse::GetStatusInfo(JNIEnv* env,
                                          int* status_code,
                                          std::string* reason_phrase) const {
  int status = Java_AwWebResourceResponse_getStatusCode(env, java_object_);
  ScopedJavaLocalRef<jstring> jstring_reason_phrase =
      Java_AwWebResourceResponse_getReasonPhrase(env, java_object_);
  if (status < 100 || status >= 600 || jstring_reason_phrase.is_null())
    return false;
  *status_code = status;
  *reason_phrase = ConvertJavaStringToUTF8(jstring_reason_phrase);
  return true;
}

bool AwWebResourceResponse::GetResponseHeaders(
    JNIEnv* env,
    net::HttpResponseHeaders* headers) const {
  ScopedJavaLocalRef<jobjectArray> jstringArray_headerNames =
      Java_AwWebResourceResponse_getResponseHeaderNames(env, java_object_);
  ScopedJavaLocalRef<jobjectArray> jstringArray_headerValues =
      Java_AwWebResourceResponse_getResponseHeaderValues(env, java_object_);
  if (jstringArray_headerNames.is_null() || jstringArray_headerValues.is_null())
    return false;
  std::vector<std::string> header_names;
  std::vector<std::string> header_values;
  AppendJavaStringArrayToStringVector(env, jstringArray_headerNames,
                                      &header_names);
  AppendJavaStringArrayToStringVector(env, jstringArray_headerValues,
                                      &header_values);
  DCHECK_EQ(header_values.size(), header_names.size());
  for (size_t i = 0; i < header_names.size(); ++i) {
    std::string header_line(header_names[i]);
    header_line.append(": ");
    header_line.append(header_values[i]);
    headers->AddHeader(header_line);
  }
  return true;
}

}  // namespace android_webview
