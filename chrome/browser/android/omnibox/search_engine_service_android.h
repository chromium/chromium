// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OMNIBOX_SEARCH_ENGINE_SERVICE_ANDROID_H_
#define CHROME_BROWSER_ANDROID_OMNIBOX_SEARCH_ENGINE_SERVICE_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

class Profile;
struct AiModeButtonUiConfig;

// C++ native peer for SearchEngineService.java.
//
// Observes AiModeButtonService and AimEligibilityService, evaluates eligibility
// gating (OmniboxFieldTrial::IsAimOmniboxEntrypointEnabled), and pushes
// AiModeButtonUiConfig updates to the Java service whenever configuration or
// eligibility changes.
class SearchEngineServiceAndroid {
 public:
  SearchEngineServiceAndroid(JNIEnv* env,
                             const base::android::JavaRef<jobject>& obj,
                             Profile* profile);
  ~SearchEngineServiceAndroid();
  SearchEngineServiceAndroid(const SearchEngineServiceAndroid&) = delete;
  SearchEngineServiceAndroid& operator=(const SearchEngineServiceAndroid&) =
      delete;

  void Destroy(JNIEnv* env);

 private:
  void OnAiModeButtonConfigChanged(const AiModeButtonUiConfig* config);
  void OnEligibilityChanged();

  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
  raw_ptr<Profile> profile_;
  base::CallbackListSubscription ai_mode_config_subscription_;
  base::CallbackListSubscription aim_eligibility_subscription_;
  base::WeakPtrFactory<SearchEngineServiceAndroid> weak_factory_{this};
};

#endif  // CHROME_BROWSER_ANDROID_OMNIBOX_SEARCH_ENGINE_SERVICE_ANDROID_H_
