// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/autofill_assistant/trigger_script_bridge_android.h"

#include "chrome/android/features/autofill_assistant/jni_headers/AssistantTriggerScriptBridge_jni.h"
#include "chrome/common/channel_info.h"
#include "components/autofill_assistant/browser/service/api_key_fetcher.h"
#include "components/autofill_assistant/browser/service/server_url_fetcher.h"
#include "components/autofill_assistant/browser/service/service_request_sender.h"
#include "components/autofill_assistant/browser/service/simple_url_loader_factory.h"
#include "components/autofill_assistant/browser/trigger_scripts/dynamic_trigger_conditions.h"
#include "components/autofill_assistant/browser/trigger_scripts/static_trigger_conditions.h"
#include "components/autofill_assistant/browser/web/web_controller.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

using base::android::AttachCurrentThread;

namespace autofill_assistant {

TriggerScriptBridgeAndroid::TriggerScriptBridgeAndroid() = default;
TriggerScriptBridgeAndroid::~TriggerScriptBridgeAndroid() = default;

void TriggerScriptBridgeAndroid::StartTriggerScript(
    Client* client,
    const base::android::JavaParamRef<jobject>& jdelegate,
    const GURL& initial_url,
    std::unique_ptr<TriggerContext> trigger_context) {
  DCHECK(!java_object_);
  java_object_ = base::android::ScopedJavaGlobalRef<jobject>(jdelegate);
  Java_AssistantTriggerScriptBridge_setNativePtr(
      AttachCurrentThread(), java_object_, reinterpret_cast<intptr_t>(this));

  ServerUrlFetcher url_fetcher{ServerUrlFetcher::GetDefaultServerUrl()};
  trigger_script_coordinator_ = std::make_unique<TriggerScriptCoordinator>(
      client,
      WebController::CreateForWebContents(client->GetWebContents(),
                                          &client_settings_),
      std::make_unique<ServiceRequestSender>(
          client->GetWebContents()->GetBrowserContext(),
          /* access_token_fetcher = */ nullptr,
          std::make_unique<NativeURLLoaderFactory>(),
          ApiKeyFetcher().GetAPIKey(chrome::GetChannel()),
          /* auth_enabled = */ false,
          /* disable_auth_if_no_access_token = */ true),
      url_fetcher.GetTriggerScriptsEndpoint(),
      std::make_unique<StaticTriggerConditions>(),
      std::make_unique<DynamicTriggerConditions>(), ukm::UkmRecorder::Get());

  trigger_script_coordinator_->AddObserver(this);
  trigger_script_coordinator_->Start(initial_url, std::move(trigger_context));
}

void TriggerScriptBridgeAndroid::StopTriggerScript() {
  if (java_object_) {
    Java_AssistantTriggerScriptBridge_clearNativePtr(AttachCurrentThread(),
                                                     java_object_);
    java_object_ = nullptr;
  }
  trigger_script_coordinator_ = nullptr;
}

void TriggerScriptBridgeAndroid::OnTriggerScriptAction(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jint action) {
  if (!trigger_script_coordinator_) {
    return;
  }
}

void TriggerScriptBridgeAndroid::OnBottomSheetClosedWithSwipe(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  if (!trigger_script_coordinator_) {
    return;
  }
}

void TriggerScriptBridgeAndroid::OnBackButtonPressed(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  if (!trigger_script_coordinator_) {
    return;
  }
}

void TriggerScriptBridgeAndroid::OnFeedbackButtonClicked(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  if (!trigger_script_coordinator_) {
    return;
  }
}

void TriggerScriptBridgeAndroid::OnTriggerScriptShown(
    const TriggerScriptUIProto& proto) {
  if (!java_object_) {
    return;
  }
  Java_AssistantTriggerScriptBridge_showTriggerScript(AttachCurrentThread(),
                                                      java_object_);
}

void TriggerScriptBridgeAndroid::OnTriggerScriptHidden() {
  if (!java_object_) {
    return;
  }
  Java_AssistantTriggerScriptBridge_hideTriggerScript(AttachCurrentThread(),
                                                      java_object_);
}

void TriggerScriptBridgeAndroid::OnTriggerScriptFinished(
    Metrics::LiteScriptFinishedState state) {
  if (!java_object_) {
    return;
  }
  Java_AssistantTriggerScriptBridge_onTriggerScriptFinished(
      AttachCurrentThread(), java_object_, static_cast<int>(state));
}

}  // namespace autofill_assistant
