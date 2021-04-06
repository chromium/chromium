// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "chrome/android/features/autofill_assistant/jni_headers_public/TriggerContext_jni.h"
#include "chrome/browser/android/autofill_assistant/ui_controller_android_utils.h"
#include "components/autofill_assistant/browser/startup_util.h"

namespace autofill_assistant {

static jlong JNI_TriggerContext_CreateNative(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& jexperiment_ids,
    const base::android::JavaParamRef<jobjectArray>& jparameter_names,
    const base::android::JavaParamRef<jobjectArray>& jparameter_values,
    jboolean is_cct,
    jboolean is_direct_action,
    const base::android::JavaParamRef<jstring>& jinitial_url) {
  auto trigger_context = ui_controller_android_utils::CreateTriggerContext(
      env, jexperiment_ids, jparameter_names, jparameter_values, is_cct,
      /* onboarding_shown = */ false, is_direct_action, jinitial_url);
  auto* trigger_context_ptr = trigger_context.release();
  return reinterpret_cast<jlong>(trigger_context_ptr);
}

static void JNI_TriggerContext_DestroyNative(JNIEnv* env,
                                             jlong jnative_trigger_context) {
  auto* trigger_context = static_cast<TriggerContext*>(
      reinterpret_cast<void*>(jnative_trigger_context));
  delete trigger_context;
}

static jboolean JNI_TriggerContext_IsValid(
    JNIEnv* env,
    jlong jnative_trigger_context,
    jboolean msbb_setting_enabled,
    jboolean proactive_help_setting_enabled,
    jboolean feature_module_installed) {
  auto* trigger_context = static_cast<TriggerContext*>(
      reinterpret_cast<void*>(jnative_trigger_context));
  DCHECK(trigger_context);

  // Validating trigger contexts is not yet possible for direct actions.
  DCHECK(!trigger_context->GetDirectAction());

  switch (StartupUtil().ChooseStartupModeForIntent(
      *trigger_context, {msbb_setting_enabled, proactive_help_setting_enabled,
                         feature_module_installed})) {
    case StartupUtil::StartupMode::FEATURE_DISABLED:
    case StartupUtil::StartupMode::MANDATORY_PARAMETERS_MISSING:
    case StartupUtil::StartupMode::SETTING_DISABLED:
      return false;
    case StartupUtil::StartupMode::START_REGULAR:
    case StartupUtil::StartupMode::START_BASE64_TRIGGER_SCRIPT:
    case StartupUtil::StartupMode::START_RPC_TRIGGER_SCRIPT:
      return true;
  }
}

}  // namespace autofill_assistant
