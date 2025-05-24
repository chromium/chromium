// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include <vector>

#include "base/android/jni_android.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service_factory.h"
#include "chrome/browser/navigation_predictor/search_engine_preconnector.h"
#include "chrome/browser/navigation_predictor/search_engine_preconnector_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/NavigationPredictorBridge_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;

namespace {
SearchEnginePreconnector* GetSearchEnginePreconnector(Profile* profile) {
  if (SearchEnginePreconnector::ShouldBeEnabledAsKeyedService()) {
    return SearchEnginePreconnectorKeyedServiceFactory::GetForProfile(profile);
  }

  NavigationPredictorKeyedService* navigation_predictor_service =
      NavigationPredictorKeyedServiceFactory::GetForProfile(profile);
  if (!navigation_predictor_service)
    return nullptr;
  return navigation_predictor_service->search_engine_preconnector();
}
}  // namespace

static void JNI_NavigationPredictorBridge_OnColdStart(JNIEnv* env,
                                                      Profile* profile) {
  SearchEnginePreconnector* search_engine_preconnector =
      GetSearchEnginePreconnector(profile);
  if (!search_engine_preconnector) {
    return;
  }
  search_engine_preconnector->StartPreconnecting(/*with_startup_delay=*/true);
}

static void JNI_NavigationPredictorBridge_OnActivityWarmResumed(
    JNIEnv* env,
    Profile* profile) {
  SearchEnginePreconnector* search_engine_preconnector =
      GetSearchEnginePreconnector(profile);
  if (!search_engine_preconnector) {
    return;
  }
  search_engine_preconnector->StartPreconnecting(/*with_startup_delay=*/false);
}

static void JNI_NavigationPredictorBridge_OnPause(JNIEnv* env,
                                                  Profile* profile) {
  SearchEnginePreconnector* search_engine_preconnector =
      GetSearchEnginePreconnector(profile);
  if (!search_engine_preconnector) {
    return;
  }
  search_engine_preconnector->StopPreconnecting();
}
