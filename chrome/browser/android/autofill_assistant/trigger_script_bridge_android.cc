// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/autofill_assistant/trigger_script_bridge_android.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/base64url.h"
#include "chrome/android/features/autofill_assistant/jni_headers/AssistantTriggerScriptBridge_jni.h"
#include "chrome/browser/android/autofill_assistant/assistant_header_model.h"
#include "chrome/browser/android/autofill_assistant/ui_controller_android_utils.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/common/channel_info.h"
#include "components/autofill_assistant/browser/onboarding_result.h"
#include "components/autofill_assistant/browser/service/api_key_fetcher.h"
#include "components/autofill_assistant/browser/service/server_url_fetcher.h"
#include "components/autofill_assistant/browser/service/service_request_sender_impl.h"
#include "components/autofill_assistant/browser/service/service_request_sender_local_impl.h"
#include "components/autofill_assistant/browser/service/simple_url_loader_factory.h"
#include "components/autofill_assistant/browser/string_conversions_util.h"
#include "components/autofill_assistant/browser/trigger_scripts/dynamic_trigger_conditions.h"
#include "components/autofill_assistant/browser/trigger_scripts/static_trigger_conditions.h"
#include "components/autofill_assistant/browser/web/web_controller.h"
#include "components/autofill_assistant/browser/website_login_manager_impl.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ToJavaArrayOfStrings;
using base::android::ToJavaIntArray;

namespace {
bool IsFirstTimeTriggerScriptUser() {
  return autofill_assistant::
      Java_AssistantTriggerScriptBridge_isFirstTimeTriggerScriptUser(
          AttachCurrentThread());
}
}  // namespace

namespace autofill_assistant {

TriggerScriptBridgeAndroid::TriggerScriptBridgeAndroid() = default;
TriggerScriptBridgeAndroid::~TriggerScriptBridgeAndroid() = default;

void TriggerScriptBridgeAndroid::StartTriggerScript(
    content::WebContents* web_contents,
    const JavaParamRef<jobject>& jdelegate,
    const GURL& initial_url,
    std::unique_ptr<TriggerContext> trigger_context,
    jlong jservice_request_sender) {
  DCHECK(!java_object_);
  java_object_ = ScopedJavaGlobalRef<jobject>(jdelegate);
  Java_AssistantTriggerScriptBridge_setNativePtr(
      AttachCurrentThread(), java_object_, reinterpret_cast<intptr_t>(this));

  std::unique_ptr<ServiceRequestSender> service_request_sender = nullptr;
  if (jservice_request_sender) {
    service_request_sender.reset(static_cast<ServiceRequestSender*>(
        reinterpret_cast<void*>(jservice_request_sender)));
    // TODO(b/171776026): consider exposing this in proto.
    disable_header_animations_for_testing_ = true;
  } else if (trigger_context->GetScriptParameters()
                 .GetBase64TriggerScriptsResponseProto()
                 .has_value()) {
    std::string response;
    if (!base::Base64UrlDecode(trigger_context->GetScriptParameters()
                                   .GetBase64TriggerScriptsResponseProto()
                                   .value(),
                               base::Base64UrlDecodePolicy::IGNORE_PADDING,
                               &response)) {
      LOG(ERROR) << "Failed to base64-decode trigger scripts response";
      Metrics::RecordLiteScriptFinished(
          ukm::UkmRecorder::Get(), web_contents, UNSPECIFIED_TRIGGER_UI_TYPE,
          Metrics::LiteScriptFinishedState::LITE_SCRIPT_BASE64_DECODING_ERROR);
      return;
    }
    service_request_sender =
        std::make_unique<ServiceRequestSenderLocalImpl>(response);
  } else {
    service_request_sender = std::make_unique<ServiceRequestSenderImpl>(
        web_contents->GetBrowserContext(),
        /* access_token_fetcher = */ nullptr,
        std::make_unique<NativeURLLoaderFactory>(),
        ApiKeyFetcher().GetAPIKey(chrome::GetChannel()),
        /* auth_enabled = */ false,
        /* disable_auth_if_no_access_token = */ true);
  }

  ServerUrlFetcher url_fetcher{ServerUrlFetcher::GetDefaultServerUrl()};
  if (!website_login_manager_) {
    website_login_manager_ = std::make_unique<WebsiteLoginManagerImpl>(
        ChromePasswordManagerClient::FromWebContents(web_contents),
        web_contents);
  }
  trigger_script_coordinator_ = std::make_unique<TriggerScriptCoordinator>(
      web_contents, website_login_manager_.get(),
      base::BindRepeating(&IsFirstTimeTriggerScriptUser),
      WebController::CreateForWebContents(web_contents),
      std::move(service_request_sender),
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
    const JavaParamRef<jobject>& jcaller,
    jint action) {
  if (!trigger_script_coordinator_) {
    return;
  }
  trigger_script_coordinator_->PerformTriggerScriptAction(
      static_cast<TriggerScriptProto::TriggerScriptAction>(action));
}

void TriggerScriptBridgeAndroid::OnBottomSheetClosedWithSwipe(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller) {
  if (!trigger_script_coordinator_) {
    return;
  }
  trigger_script_coordinator_->OnBottomSheetClosedWithSwipe();
}

bool TriggerScriptBridgeAndroid::OnBackButtonPressed(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller) {
  if (!trigger_script_coordinator_) {
    return false;
  }
  return trigger_script_coordinator_->OnBackButtonPressed();
}

void TriggerScriptBridgeAndroid::OnTabInteractabilityChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jboolean jinteractable) {
  if (!trigger_script_coordinator_) {
    return;
  }
  trigger_script_coordinator_->OnTabInteractabilityChanged(jinteractable);
}

void TriggerScriptBridgeAndroid::OnKeyboardVisibilityChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jboolean jvisible) {
  if (!trigger_script_coordinator_) {
    return;
  }
  trigger_script_coordinator_->OnKeyboardVisibilityChanged(jvisible);
}

void TriggerScriptBridgeAndroid::OnTriggerScriptShown(
    const TriggerScriptUIProto& proto) {
  if (!java_object_) {
    return;
  }
  JNIEnv* env = AttachCurrentThread();
  auto jheader_model =
      Java_AssistantTriggerScriptBridge_createHeaderAndGetModel(env,
                                                                java_object_);
  AssistantHeaderModel header_model(jheader_model);
  if (disable_header_animations_for_testing_) {
    header_model.SetDisableAnimations(disable_header_animations_for_testing_);
  }
  header_model.SetStatusMessage(proto.status_message());
  header_model.SetBubbleMessage(proto.callout_message());
  header_model.SetProgressVisible(proto.has_progress_bar());
  if (proto.has_progress_bar()) {
    ShowProgressBarProto::StepProgressBarConfiguration configuration;
    configuration.set_use_step_progress_bar(true);
    for (const auto& icon : proto.progress_bar().step_icons()) {
      *configuration.add_annotated_step_icons()->mutable_icon() = icon;
    }
    auto jcontext =
        Java_AssistantTriggerScriptBridge_getContext(env, java_object_);
    header_model.SetStepProgressBarConfiguration(configuration, jcontext);
    header_model.SetProgressActiveStep(proto.progress_bar().active_step());
  }

  std::vector<ChipProto> left_aligned_chips;
  std::vector<int> left_aligned_chip_actions;
  for (const auto& chip : proto.left_aligned_chips()) {
    left_aligned_chips.emplace_back(chip.chip());
    left_aligned_chip_actions.emplace_back(static_cast<int>(chip.action()));
  }
  auto jleft_aligned_chips =
      ui_controller_android_utils::CreateJavaAssistantChipList(
          env, left_aligned_chips);

  std::vector<ChipProto> right_aligned_chips;
  std::vector<int> right_aligned_chip_actions;
  for (const auto& chip : proto.right_aligned_chips()) {
    right_aligned_chips.emplace_back(chip.chip());
    right_aligned_chip_actions.emplace_back(static_cast<int>(chip.action()));
  }
  auto jright_aligned_chips =
      ui_controller_android_utils::CreateJavaAssistantChipList(
          env, right_aligned_chips);

  std::vector<std::string> cancel_popup_items;
  std::vector<int> cancel_popup_actions;
  for (const auto& choice : proto.cancel_popup().choices()) {
    cancel_popup_items.emplace_back(choice.text());
    cancel_popup_actions.emplace_back(static_cast<int>(choice.action()));
  }

  last_shown_trigger_script_ = proto;
  jboolean success = Java_AssistantTriggerScriptBridge_showTriggerScript(
      env, java_object_, ToJavaArrayOfStrings(env, cancel_popup_items),
      ToJavaIntArray(env, cancel_popup_actions), jleft_aligned_chips,
      ToJavaIntArray(env, left_aligned_chip_actions), jright_aligned_chips,
      ToJavaIntArray(env, right_aligned_chip_actions),
      proto.resize_visual_viewport(), proto.scroll_to_hide());
  trigger_script_coordinator_->OnTriggerScriptShown(success);
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
  // NOTE: for now, the transition to the regular script (if state == ACCEPTED)
  // is still done in Java.
  Java_AssistantTriggerScriptBridge_onTriggerScriptFinished(
      AttachCurrentThread(), java_object_, static_cast<int>(state));
  StopTriggerScript();
}

void TriggerScriptBridgeAndroid::OnVisibilityChanged(bool visible) {
  if (!visible || !trigger_script_coordinator_) {
    return;
  }

  // Every time the tab becomes visible again we have to double-check if the
  // proactive help settings is still enabled.
  trigger_script_coordinator_->OnProactiveHelpSettingChanged(
      Java_AssistantTriggerScriptBridge_isProactiveHelpEnabled(
          AttachCurrentThread()));
}

base::Optional<TriggerScriptUIProto>
TriggerScriptBridgeAndroid::GetLastShownTriggerScript() const {
  return last_shown_trigger_script_;
}

void TriggerScriptBridgeAndroid::ClearLastShownTriggerScript() {
  last_shown_trigger_script_.reset();
}

void TriggerScriptBridgeAndroid::OnOnboardingRequested(
    bool is_dialog_onboarding_enabled) {
  if (!java_object_) {
    return;
  }
  Java_AssistantTriggerScriptBridge_onOnboardingRequested(
      AttachCurrentThread(), java_object_, is_dialog_onboarding_enabled);
}

void TriggerScriptBridgeAndroid::OnOnboardingFinished(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jboolean jonboarding_shown,
    jint jresult) {
  if (!trigger_script_coordinator_) {
    return;
  }
  trigger_script_coordinator_->OnOnboardingFinished(
      jonboarding_shown, static_cast<OnboardingResult>(jresult));
}

}  // namespace autofill_assistant
