// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/js_java_interaction/js_reply_proxy.h"

#include <utility>

#include "base/android/jni_string.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "components/js_injection/browser/web_message_reply_proxy.h"
#include "components/js_injection/common/enum.mojom.h"
#include "components/js_injection/common/interfaces.mojom.h"
#include "content/public/browser/android/message_payload.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/JsReplyProxy_jni.h"

namespace android_webview {

namespace {
void JavaScriptResultCallback(
    const base::android::ScopedJavaGlobalRef<jobject>& callback,
    base::expected<base::Value, js_injection::mojom::JavaScriptExecutionError>
        result) {
  std::u16string j_json = u"";
  auto error = js_injection::mojom::JavaScriptExecutionError::kNotSupported;
  if (result.has_value()) {
    j_json = base::UTF8ToUTF16(base::WriteJson(result.value()).value_or(""));
  } else {
    error = result.error();
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_JsReplyProxy_onEvaluateJavaScriptResult(env, j_json, result.has_value(),
                                               error, callback);
}
}  // namespace

JsReplyProxy::JsReplyProxy(js_injection::WebMessageReplyProxy* reply_proxy)
    : reply_proxy_(reply_proxy) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_ref_.Reset(
      Java_JsReplyProxy_create(env, reinterpret_cast<intptr_t>(this)));
}

JsReplyProxy::~JsReplyProxy() {
  if (!java_ref_)
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_JsReplyProxy_onDestroy(env, java_ref_);
}

base::android::ScopedJavaLocalRef<jobject> JsReplyProxy::GetJavaPeer() {
  return base::android::ScopedJavaLocalRef<jobject>(java_ref_);
}

void JsReplyProxy::PostMessage(JNIEnv* env,
                               const base::android::JavaRef<jobject>& payload) {
  reply_proxy_->PostWebMessage(
      content::android::ConvertToWebMessagePayloadFromJava(
          base::android::ScopedJavaLocalRef<jobject>(payload)));
}

void JsReplyProxy::ExecuteJavaScript(
    JNIEnv* env,
    const std::u16string& java_script,
    const base::android::JavaRef<jobject>& callback) {
  if (!callback) {
    reply_proxy_->ExecuteJavaScript(java_script, false, base::NullCallback());
    return;
  }

  reply_proxy_->ExecuteJavaScript(
      java_script, true,
      base::BindOnce(&JavaScriptResultCallback,
                     base::android::ScopedJavaGlobalRef<jobject>(callback)));
}

}  // namespace android_webview

DEFINE_JNI(JsReplyProxy)
