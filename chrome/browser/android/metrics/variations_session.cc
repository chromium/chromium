// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "chrome/browser/browser_process.h"
#include "components/variations/service/variations_service.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/VariationsSession_jni.h"

using base::android::JavaParamRef;

namespace {

// Tracks whether VariationsService::OnAppEnterForeground() has been called
// previously, in order to set the restrict mode param before the first call.
bool g_on_app_enter_foreground_called = false;

}  // namespace

static void JNI_VariationsSession_StartVariationsSession(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    std::string& restrict_mode) {
  DCHECK(g_browser_process);

  variations::VariationsService* variations_service =
      g_browser_process->variations_service();
  // Triggers an OnAppEnterForeground on the VariationsService. This may fetch
  // a new seed.
  if (variations_service) {
    if (!restrict_mode.empty() && !g_on_app_enter_foreground_called)
      variations_service->SetRestrictMode(restrict_mode);
    variations_service->OnAppEnterForeground();
    g_on_app_enter_foreground_called = true;
  }
}

static std::string JNI_VariationsSession_GetLatestCountry(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  std::string latest_country;

  variations::VariationsService* variations_service =
      g_browser_process->variations_service();
  if (variations_service) {
    latest_country = variations_service->GetLatestCountry();
  }

  return latest_country;
}
