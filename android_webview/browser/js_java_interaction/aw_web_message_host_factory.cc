// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/js_java_interaction/aw_web_message_host_factory.h"

#include <string>

#include "android_webview/browser/js_java_interaction/js_reply_proxy.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "components/js_injection/browser/js_communication_host.h"
#include "components/js_injection/browser/web_message.h"
#include "components/js_injection/browser/web_message_host.h"
#include "components/js_injection/common/origin_matcher.h"
#include "content/public/browser/android/message_payload.h"
#include "content/public/browser/android/message_port_helper.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/WebMessageListenerHolder_jni.h"
#include "android_webview/browser_jni_headers/WebMessageListenerInfo_jni.h"

namespace android_webview {
namespace {

// Calls through to WebMessageListenerInfo.
class AwWebMessageHost : public js_injection::WebMessageHost {
 public:
  AwWebMessageHost(js_injection::WebMessageReplyProxy* reply_proxy,
                   const base::android::ScopedJavaGlobalRef<jobject>& listener,
                   const std::string& top_level_origin_string,
                   const std::string& origin_string,
                   bool is_main_frame)
      : reply_proxy_(reply_proxy),
        listener_(listener),
        top_level_origin_string_(top_level_origin_string),
        origin_string_(origin_string),
        is_main_frame_(is_main_frame) {}

  ~AwWebMessageHost() override = default;

  // js_injection::WebMessageHost:
  void OnPostMessage(
      std::unique_ptr<js_injection::WebMessage> message) override {
    JNIEnv* env = base::android::AttachCurrentThread();
    base::android::ScopedJavaLocalRef<jobjectArray> jports =
        content::android::CreateJavaMessagePort(std::move(message->ports));
    Java_WebMessageListenerHolder_onPostMessage(
        env, listener_,
        content::android::ConvertWebMessagePayloadToJava(message->message),
        base::android::ConvertUTF8ToJavaString(env, top_level_origin_string_),
        base::android::ConvertUTF8ToJavaString(env, origin_string_),
        is_main_frame_, jports, reply_proxy_.GetJavaPeer());
  }

 private:
  JsReplyProxy reply_proxy_;
  base::android::ScopedJavaGlobalRef<jobject> listener_;
  const std::string top_level_origin_string_;
  const std::string origin_string_;
  const bool is_main_frame_;
};

}  // namespace

AwWebMessageHostFactory::AwWebMessageHostFactory(
    const base::android::JavaParamRef<jobject>& listener)
    : listener_(listener) {}

AwWebMessageHostFactory::~AwWebMessageHostFactory() = default;

// static
std::vector<jni_zero::ScopedJavaLocalRef<jobject>>
AwWebMessageHostFactory::GetWebMessageListenerInfo(
    js_injection::JsCommunicationHost* host,
    JNIEnv* env) {
  auto factories = host->GetWebMessageHostFactories();
  std::vector<jni_zero::ScopedJavaLocalRef<jobject>> ret;

  for (size_t i = 0; i < factories.size(); ++i) {
    const auto& factory = factories[i];
    const std::vector<std::string> rules =
        factory.allowed_origin_rules.Serialize();
    ret.push_back(Java_WebMessageListenerInfo_create(
        env, base::android::ConvertUTF16ToJavaString(env, factory.js_name),
        base::android::ToJavaArrayOfStrings(env, rules),
        static_cast<AwWebMessageHostFactory*>(factory.factory)->listener_));
  }
  return ret;
}

std::unique_ptr<js_injection::WebMessageHost>
AwWebMessageHostFactory::CreateHost(const std::string& top_level_origin_string,
                                    const std::string& origin_string,
                                    bool is_main_frame,
                                    js_injection::WebMessageReplyProxy* proxy) {
  return std::make_unique<AwWebMessageHost>(
      proxy, listener_, top_level_origin_string, origin_string, is_main_frame);
}

}  // namespace android_webview
