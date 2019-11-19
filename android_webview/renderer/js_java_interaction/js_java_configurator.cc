// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/renderer/js_java_interaction/js_java_configurator.h"

#include "android_webview/renderer/js_java_interaction/js_binding.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace android_webview {

struct JsJavaConfigurator::JsObjectInfo {
  net::ProxyBypassRules allowed_origin_rules;
  mojo::AssociatedRemote<mojom::JsToJavaMessaging> js_to_java_messaging;
};

JsJavaConfigurator::JsJavaConfigurator(content::RenderFrame* render_frame)
    : RenderFrameObserver(render_frame) {
  render_frame->GetAssociatedInterfaceRegistry()->AddInterface(
      base::BindRepeating(&JsJavaConfigurator::BindPendingReceiver,
                          base::Unretained(this)));
}

JsJavaConfigurator::~JsJavaConfigurator() = default;

void JsJavaConfigurator::SetJsObjects(
    std::vector<mojom::JsObjectPtr> js_object_ptrs) {
  JsObjectMap js_objects;
  for (const auto& js_object : js_object_ptrs) {
    const auto& js_object_info_pair = js_objects.insert(
        {js_object->js_object_name, std::make_unique<JsObjectInfo>()});
    JsObjectInfo* js_object_info = js_object_info_pair.first->second.get();
    js_object_info->allowed_origin_rules = js_object->allowed_origin_rules;
    js_object_info->js_to_java_messaging =
        mojo::AssociatedRemote<mojom::JsToJavaMessaging>(
            std::move(js_object->js_to_java_messaging));
  }
  js_objects_.swap(js_objects);
}

void JsJavaConfigurator::DidClearWindowObject() {
  if (inside_did_clear_window_object_)
    return;

  base::AutoReset<bool> flag_entry(&inside_did_clear_window_object_, true);

  url::Origin frame_origin =
      url::Origin(render_frame()->GetWebFrame()->GetSecurityOrigin());
  std::vector<std::unique_ptr<JsBinding>> js_bindings;
  js_bindings.reserve(js_objects_.size());
  for (const auto& js_object : js_objects_) {
    if (!js_object.second->allowed_origin_rules.Matches(frame_origin.GetURL()))
      continue;
    js_bindings.push_back(
        JsBinding::Install(render_frame(), js_object.first, this));
  }
  js_bindings_.swap(js_bindings);
}

void JsJavaConfigurator::WillReleaseScriptContext(
    v8::Local<v8::Context> context,
    int32_t world_id) {
  // We created v8 global objects only in the main world, should clear them only
  // when this is for main world.
  if (world_id != content::ISOLATED_WORLD_ID_GLOBAL)
    return;

  for (const auto& js_binding : js_bindings_)
    js_binding->ReleaseV8GlobalObjects();
}

void JsJavaConfigurator::OnDestruct() {
  delete this;
}

void JsJavaConfigurator::BindPendingReceiver(
    mojo::PendingAssociatedReceiver<mojom::JsJavaConfigurator>
        pending_receiver) {
  receiver_.Bind(std::move(pending_receiver),
                 render_frame()->GetTaskRunner(
                     blink::TaskType::kInternalNavigationAssociated));
}

mojom::JsToJavaMessaging* JsJavaConfigurator::GetJsToJavaMessage(
    const base::string16& js_object_name) {
  auto iterator = js_objects_.find(js_object_name);
  if (iterator == js_objects_.end())
    return nullptr;
  return iterator->second->js_to_java_messaging.get();
}

}  // namespace android_webview
