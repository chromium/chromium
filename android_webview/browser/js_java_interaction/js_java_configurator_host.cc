// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/js_java_interaction/js_java_configurator_host.h"

#include "android_webview/browser/js_java_interaction/js_to_java_messaging.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace android_webview {

struct JsObject {
  JsObject(base::string16 name,
           net::ProxyBypassRules allowed_origin_rules,
           const base::android::JavaRef<jobject>& listener)
      : name_(std::move(name)),
        allowed_origin_rules_(std::move(allowed_origin_rules)),
        listener_ref_(listener) {}

  JsObject(JsObject&& other)
      : name_(std::move(other.name_)),
        allowed_origin_rules_(std::move(other.allowed_origin_rules_)),
        listener_ref_(std::move(other.listener_ref_)) {}

  JsObject& operator=(JsObject&& other) {
    name_ = std::move(other.name_);
    allowed_origin_rules_ = std::move(other.allowed_origin_rules_);
    listener_ref_ = std::move(other.listener_ref_);
    return *this;
  }

  ~JsObject() = default;

  base::string16 name_;
  net::ProxyBypassRules allowed_origin_rules_;
  base::android::ScopedJavaGlobalRef<jobject> listener_ref_;

  DISALLOW_COPY_AND_ASSIGN(JsObject);
};

JsJavaConfiguratorHost::JsJavaConfiguratorHost(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

JsJavaConfiguratorHost::~JsJavaConfiguratorHost() = default;

base::android::ScopedJavaLocalRef<jstring>
JsJavaConfiguratorHost::AddWebMessageListener(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& listener,
    const base::android::JavaParamRef<jstring>& js_object_name,
    const base::android::JavaParamRef<jobjectArray>& allowed_origin_rules) {
  base::string16 native_js_object_name =
      base::android::ConvertJavaStringToUTF16(env, js_object_name);

  std::vector<std::string> native_allowed_origin_rule_strings;
  AppendJavaStringArrayToStringVector(env, allowed_origin_rules,
                                      &native_allowed_origin_rule_strings);

  net::ProxyBypassRules native_allowed_origin_rules;
  // We don't want to inject js object to origins that matches implicit rules
  // automatically. Later rules override earilier rules, so we add subtracing
  // rules first.
  native_allowed_origin_rules.AddRulesToSubtractImplicit();
  for (auto& rule : native_allowed_origin_rule_strings) {
    if (!native_allowed_origin_rules.AddRuleFromString(rule)) {
      return base::android::ConvertUTF8ToJavaString(
          env, "allowedOriginRules " + rule + " is invalid");
    }
  }

  for (const auto& js_object : js_objects_) {
    if (js_object.name_ == native_js_object_name) {
      return base::android::ConvertUTF16ToJavaString(
          env, base::ASCIIToUTF16("jsObjectName ") + js_object.name_ +
                   base::ASCIIToUTF16(" is already added."));
    }
  }

  js_objects_.emplace_back(native_js_object_name, native_allowed_origin_rules,
                           listener);

  web_contents()->ForEachFrame(base::BindRepeating(
      &JsJavaConfiguratorHost::NotifyFrame, base::Unretained(this)));
  return nullptr;
}

void JsJavaConfiguratorHost::RemoveWebMessageListener(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& js_object_name) {
  base::string16 native_js_object_name =
      ConvertJavaStringToUTF16(env, js_object_name);

  for (auto iterator = js_objects_.begin(); iterator != js_objects_.end();
       ++iterator) {
    if (iterator->name_ == native_js_object_name) {
      js_objects_.erase(iterator);
      web_contents()->ForEachFrame(base::BindRepeating(
          &JsJavaConfiguratorHost::NotifyFrame, base::Unretained(this)));
      break;
    }
  }
}

void JsJavaConfiguratorHost::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  NotifyFrame(render_frame_host);
}

void JsJavaConfiguratorHost::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  js_to_java_messagings_.erase(render_frame_host);
}

void JsJavaConfiguratorHost::NotifyFrame(
    content::RenderFrameHost* render_frame_host) {
  mojo::AssociatedRemote<mojom::JsJavaConfigurator> configurator_remote;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
      &configurator_remote);
  std::vector<mojom::JsObjectPtr> js_objects;
  js_objects.reserve(js_objects_.size());
  for (const auto& js_object : js_objects_) {
    mojo::PendingAssociatedRemote<mojom::JsToJavaMessaging> pending_remote;
    js_to_java_messagings_[render_frame_host].emplace_back(
        std::make_unique<JsToJavaMessaging>(
            render_frame_host,
            pending_remote.InitWithNewEndpointAndPassReceiver(),
            js_object.listener_ref_, js_object.allowed_origin_rules_));
    js_objects.push_back(mojom::JsObject::New(js_object.name_,
                                              std::move(pending_remote),
                                              js_object.allowed_origin_rules_));
  }
  configurator_remote->SetJsObjects(std::move(js_objects));
}

}  // namespace android_webview
