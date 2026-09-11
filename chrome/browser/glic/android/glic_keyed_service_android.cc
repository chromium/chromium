// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/android/glic_keyed_service_android.h"

#include <vector>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/public/glic_instance_id.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_passkeys.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/android/browser_context_handle.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/glic/android/jni_headers/GlicKeyedServiceImpl_jni.h"

using base::android::AttachCurrentThread;

using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace glic {
namespace {
const char kGlicKeyedServiceAndroidKey[] = "glic_keyed_service_android";
}  // namespace

template <mojom::InvocationSource Source>
class AndroidAutoSubmitPasskeyHelper {
 public:
  static InvokeWithAutoSubmitPasskey GetPassKey() {
    return InvokeWithAutoSubmitPasskeyProvider::GetPassKey();
  }
};

// static
ScopedJavaLocalRef<jobject> GlicKeyedService::GetJavaObject(
    GlicKeyedService* service) {
  if (!service->GetUserData(kGlicKeyedServiceAndroidKey)) {
    service->SetUserData(kGlicKeyedServiceAndroidKey,
                         std::make_unique<GlicKeyedServiceAndroid>(service));
  }

  GlicKeyedServiceAndroid* bridge = static_cast<GlicKeyedServiceAndroid*>(
      service->GetUserData(kGlicKeyedServiceAndroidKey));

  return bridge->GetJavaObject();
}

GlicKeyedServiceAndroid::GlicKeyedServiceAndroid(GlicKeyedService* service)
    : service_(service) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(env, Java_GlicKeyedServiceImpl_create(
                           env, reinterpret_cast<int64_t>(this)));
  global_show_hide_subscription_ =
      service_->instance_coordinator().AddGlobalShowHideCallback(
          base::BindRepeating(&GlicKeyedServiceAndroid::OnGlobalShowHide,
                              base::Unretained(this)));
  web_actuation_pref_subscription_ =
      service_->enabling().RegisterOnUserEnabledActuationOnWebChanged(
          base::BindRepeating(
              &GlicKeyedServiceAndroid::OnUserEnabledActuationOnWebChanged,
              base::Unretained(this)));
  experimental_triggering_pref_subscription_ =
      service_->enabling().RegisterOnExperimentalTriggeringEnabledChanged(
          base::BindRepeating(
              &GlicKeyedServiceAndroid::OnExperimentalTriggeringEnabledChanged,
              base::Unretained(this)));
  allowed_changed_subscription_ = service_->enabling().RegisterAllowedChanged(
      base::BindRepeating(&GlicKeyedServiceAndroid::OnAllowedStateChanged,
                          base::Unretained(this)));
}

GlicKeyedServiceAndroid::~GlicKeyedServiceAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_GlicKeyedServiceImpl_onNativeDestroyed(env, java_obj_);
}

base::android::ScopedJavaLocalRef<jobject>
GlicKeyedServiceAndroid::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(java_obj_);
}

void GlicKeyedServiceAndroid::ToggleUI(JNIEnv* env,
                                       int64_t browser_window_ptr,
                                       bool prevent_close,
                                       Profile* profile,
                                       int32_t source) {
  auto* window = reinterpret_cast<BrowserWindowInterface*>(browser_window_ptr);
  CHECK(window);

  service_->ToggleUI(window, prevent_close,
                     static_cast<mojom::InvocationSource>(source));
}

bool GlicKeyedServiceAndroid::InvokeWithAutoSubmit(JNIEnv* env,
                                                   TabAndroid* tab,
                                                   std::string text,
                                                   int32_t source) {
  if (!tab) {
    return false;
  }

  auto invocation_source = static_cast<mojom::InvocationSource>(source);
  GlicInvokeOptions options(Target(*tab), invocation_source);
  options.prompts.push_back(std::move(text));

  switch (invocation_source) {
    case mojom::InvocationSource::kUniversalCart:
      service_->InvokeWithAutoSubmit(
          AndroidAutoSubmitPasskeyHelper<
              mojom::InvocationSource::kUniversalCart>::GetPassKey(),
          std::move(options));
      return true;
    default:
      // Handle unauthorized source
      return false;
  }
}

void GlicKeyedServiceAndroid::InvokeWithPrompt(JNIEnv* env,
                                               TabAndroid* tab,
                                               std::string text,
                                               int32_t source) {
  if (!tab) {
    return;
  }

  GlicInvokeOptions options(Target(*tab),
                            static_cast<mojom::InvocationSource>(source));
  options.prompts.push_back(std::move(text));
  service_->Invoke(std::move(options));
}

void GlicKeyedServiceAndroid::Invoke(JNIEnv* env,
                                     TabAndroid* tab,
                                     int32_t source) {
  if (!tab) {
    return;
  }

  GlicInvokeOptions options(Target(*tab),
                            static_cast<mojom::InvocationSource>(source));
  service_->Invoke(std::move(options));
}

void GlicKeyedServiceAndroid::InvokeWithConversation(
    JNIEnv* env,
    TabAndroid* tab,
    std::string glic_conversation_id,
    int32_t source) {
  if (glic_conversation_id.empty()) {
    return;
  }
  Target target =
      tab ? Target(*tab, ConversationId(std::move(glic_conversation_id)))
          : Target(ConversationId(std::move(glic_conversation_id)));
  GlicInvokeOptions options(std::move(target),
                            static_cast<mojom::InvocationSource>(source));
  service_->Invoke(std::move(options));
}

bool GlicKeyedServiceAndroid::IsPanelShowingForBrowser(
    JNIEnv* env,
    int64_t browser_window_ptr) {
  auto* window = reinterpret_cast<BrowserWindowInterface*>(browser_window_ptr);
  CHECK(window);
  return service_->IsPanelShowingForBrowser(*window);
}

bool GlicKeyedServiceAndroid::GetUserEnabledActuationOnWeb(JNIEnv* env) {
  return service_->enabling().GetUserEnabledActuationOnWeb();
}

void GlicKeyedServiceAndroid::SetUserEnabledActuationOnWeb(JNIEnv* env,
                                                           bool enabled) {
  service_->enabling().SetUserEnabledActuationOnWeb(enabled);
}

bool GlicKeyedServiceAndroid::GetExperimentalTriggeringEnabled(JNIEnv* env) {
  return service_->enabling().GetExperimentalTriggeringEnabled();
}

void GlicKeyedServiceAndroid::SetExperimentalTriggeringEnabled(JNIEnv* env,
                                                               bool enabled) {
  service_->enabling().SetExperimentalTriggeringEnabled(enabled);
}

void GlicKeyedServiceAndroid::ShareTabs(JNIEnv* env,
                                        std::vector<TabAndroid*> tabs,
                                        std::string instance_id,
                                        bool new_conversation,
                                        int32_t source) {
  if (tabs.empty() || !tabs[0]) {
    return;
  }
  TabAndroid* target_tab = tabs[0];
  auto invocation_source = static_cast<mojom::InvocationSource>(source);

  GlicInvokeOptions options =
      new_conversation
          ? GlicInvokeOptions(Target(*target_tab, NewConversation()),
                              invocation_source)
          : GlicInvokeOptions(Target(*target_tab, InstanceId(instance_id)),
                              invocation_source);

  std::vector<tabs::TabHandle> tab_handles;
  tab_handles.reserve(tabs.size());
  for (TabAndroid* tab : tabs) {
    if (tab) {
      tab_handles.push_back(tab->GetHandle());
    }
  }

  if (new_conversation) {
    base::UmaHistogramCounts100(
        "Glic.TabContextMenu.PinnedTabsToNewConversation", tab_handles.size());
  } else {
    base::UmaHistogramCounts100(
        "Glic.TabContextMenu.PinnedTabsToExistingConversation",
        tab_handles.size());
  }

  options.tab_sharing =
      TabSharingOptions(std::move(tab_handles), GlicPinTrigger::kContextMenu);
  service_->instance_coordinator().Invoke(std::move(options));
}

void GlicKeyedServiceAndroid::UnshareTabs(JNIEnv* env,
                                          std::vector<TabAndroid*> tabs) {
  std::vector<tabs::TabHandle> tab_handles;
  tab_handles.reserve(tabs.size());
  for (TabAndroid* tab : tabs) {
    if (tab) {
      tab_handles.push_back(tab->GetHandle());
    }
  }
  if (tab_handles.empty()) {
    return;
  }
  service_->instance_coordinator().UnpinTabsFromAllInstances(
      tab_handles, GlicUnpinTrigger::kContextMenu);
}

bool GlicKeyedServiceAndroid::IsTabPinnedToAnyInstance(
    JNIEnv* env,
    std::vector<TabAndroid*> tabs) {
  for (TabAndroid* tab : tabs) {
    if (tab && service_->instance_coordinator().IsTabPinnedToAnyInstance(
                   tab->GetHandle())) {
      return true;
    }
  }
  return false;
}

std::vector<std::string> GlicKeyedServiceAndroid::GetRecentlyActiveInstances(
    JNIEnv* env,
    int32_t limit) {
  std::vector<ConversationInfo> infos =
      service_->instance_coordinator().GetRecentlyActiveInstances(
          static_cast<size_t>(limit), base::TimeDelta::Max());
  std::vector<std::string> flattened;
  flattened.reserve(infos.size() * 2);
  for (const ConversationInfo& info : infos) {
    flattened.push_back(info.instance_id.value());
    flattened.push_back(info.title);
  }
  return flattened;
}

void GlicKeyedServiceAndroid::OnGlobalShowHide() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_GlicKeyedServiceImpl_onGlobalShowHide(env, java_obj_);
}

void GlicKeyedServiceAndroid::OnUserEnabledActuationOnWebChanged() {
  JNIEnv* env = base::android::AttachCurrentThread();
  bool enabled = service_->enabling().GetUserEnabledActuationOnWeb();
  Java_GlicKeyedServiceImpl_onUserEnabledActuationOnWebChanged(env, java_obj_,
                                                               enabled);
}

void GlicKeyedServiceAndroid::OnExperimentalTriggeringEnabledChanged() {
  JNIEnv* env = base::android::AttachCurrentThread();
  bool enabled = service_->enabling().GetExperimentalTriggeringEnabled();
  Java_GlicKeyedServiceImpl_onExperimentalTriggeringEnabledChanged(
      env, java_obj_, enabled);
}

void GlicKeyedServiceAndroid::OnAllowedStateChanged() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_GlicKeyedServiceImpl_onAllowedStateChanged(env, java_obj_);
}

bool GlicKeyedService::IsGlicShortcutActive() {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> java_service = GetJavaObject(this);
  if (!java_service) {
    return false;
  }
  return Java_GlicKeyedServiceImpl_isGlicShortcutActive(
      env, java_service, profile_->GetJavaObject());
}

bool GlicKeyedService::IsBottomBarEnabled() {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> java_service = GetJavaObject(this);
  if (!java_service) {
    return false;
  }
  return Java_GlicKeyedServiceImpl_isBottomBarEnabled(env, java_service);
}

}  // namespace glic

DEFINE_JNI(GlicKeyedServiceImpl)
