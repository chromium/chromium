// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_JS_JAVA_INTERACTION_AW_WEB_MESSAGE_HOST_FACTORY_H_
#define ANDROID_WEBVIEW_BROWSER_JS_JAVA_INTERACTION_AW_WEB_MESSAGE_HOST_FACTORY_H_

#include <vector>

#include "base/android/scoped_java_ref.h"
#include "components/js_injection/browser/web_message_host_factory.h"

namespace js_injection {
class JsCommunicationHost;
}

namespace android_webview {

// Adapts WebMessageHostFactory for use by WebView. An AwWebMessageHostFactory
// is created per WebMessageListener. More specifically, every call to
// AwContents::AddWebMessageListener() creates a new AwWebMessageHostFactory.
class AwWebMessageHostFactory : public js_injection::WebMessageHostFactory {
 public:
  explicit AwWebMessageHostFactory(
      const base::android::JavaParamRef<jobject>& listener);
  AwWebMessageHostFactory(const AwWebMessageHostFactory&) = delete;
  AwWebMessageHostFactory& operator=(const AwWebMessageHostFactory&) = delete;
  ~AwWebMessageHostFactory() override;

  // Returns an array of WebMessageListenerInfos based on the registered
  // factories.
  static std::vector<jni_zero::ScopedJavaLocalRef<jobject>>
  GetWebMessageListenerInfo(js_injection::JsCommunicationHost* host,
                            JNIEnv* env);

  // js_injection::WebMessageConnection:
  std::unique_ptr<js_injection::WebMessageHost> CreateHost(
      const std::string& top_level_origin_string,
      const std::string& origin_string,
      bool is_main_frame,
      js_injection::WebMessageReplyProxy* proxy) override;

 private:
  // The WebMessageListenerHost that was supplied to
  // AwContents::AddWebMessageListener().
  base::android::ScopedJavaGlobalRef<jobject> listener_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_JS_JAVA_INTERACTION_AW_WEB_MESSAGE_HOST_FACTORY_H_
