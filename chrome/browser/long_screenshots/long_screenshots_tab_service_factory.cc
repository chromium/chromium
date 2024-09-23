// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/long_screenshots/long_screenshots_tab_service_factory.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "build/build_config.h"
#include "chrome/browser/long_screenshots/long_screenshots_tab_service.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/keyed_service/core/simple_dependency_manager.h"
#include "components/keyed_service/core/simple_factory_key.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/LongScreenshotsTabServiceFactory_jni.h"

namespace long_screenshots {

namespace {

constexpr char kFeatureDirname[] = "long_screenshots_tab_service";

}  // namespace

// static
LongScreenshotsTabServiceFactory*
LongScreenshotsTabServiceFactory::GetInstance() {
  static base::NoDestructor<LongScreenshotsTabServiceFactory> instance;
  return instance.get();
}

// static
long_screenshots::LongScreenshotsTabService*
LongScreenshotsTabServiceFactory::GetServiceInstance(SimpleFactoryKey* key) {
  return static_cast<long_screenshots::LongScreenshotsTabService*>(
      GetInstance()->GetServiceForKey(key, true));
}

LongScreenshotsTabServiceFactory::LongScreenshotsTabServiceFactory()
    : SimpleKeyedServiceFactory("LongScreenshotsTabService",
                                SimpleDependencyManager::GetInstance()) {}

LongScreenshotsTabServiceFactory::~LongScreenshotsTabServiceFactory() = default;

std::unique_ptr<KeyedService>
LongScreenshotsTabServiceFactory::BuildServiceInstanceFor(
    SimpleFactoryKey* key) const {
  // Prevent this working off the record.
  if (key->IsOffTheRecord())
    return nullptr;

  return std::make_unique<LongScreenshotsTabService>(
      std::make_unique<paint_preview::PaintPreviewFileMixin>(key->GetPath(),
                                                             kFeatureDirname),
      nullptr, key->IsOffTheRecord());
}

SimpleFactoryKey* LongScreenshotsTabServiceFactory::GetKeyToUse(
    SimpleFactoryKey* key) const {
  return key;
}

base::android::ScopedJavaLocalRef<jobject>
JNI_LongScreenshotsTabServiceFactory_GetServiceInstanceForCurrentProfile(
    JNIEnv* env) {
  ProfileKey* profile_key =
      ProfileManager::GetLastUsedProfile()->GetProfileKey();
  base::android::ScopedJavaGlobalRef<jobject> java_ref =
      LongScreenshotsTabServiceFactory::GetServiceInstance(profile_key)
          ->GetJavaRef();
  return base::android::ScopedJavaLocalRef<jobject>(java_ref);
}

}  // namespace long_screenshots
