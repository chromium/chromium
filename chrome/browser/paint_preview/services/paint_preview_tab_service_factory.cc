// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/paint_preview/services/paint_preview_tab_service_factory.h"

#include <utility>

#include "build/build_config.h"
#include "chrome/browser/paint_preview/services/paint_preview_tab_service.h"
#include "chrome/browser/paint_preview/services/paint_preview_tab_service_file_mixin.h"
#include "components/keyed_service/core/simple_dependency_manager.h"
#include "components/keyed_service/core/simple_factory_key.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/paint_preview/android/jni_headers/PaintPreviewTabServiceFactory_jni.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/profiles/profile_manager.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace paint_preview {

namespace {

constexpr char kFeatureDirname[] = "tab_service";

}  // namespace

// static
PaintPreviewTabServiceFactory* PaintPreviewTabServiceFactory::GetInstance() {
  static base::NoDestructor<PaintPreviewTabServiceFactory> instance;
  return instance.get();
}

// static
paint_preview::PaintPreviewTabService*
PaintPreviewTabServiceFactory::GetServiceInstance(SimpleFactoryKey* key) {
  return static_cast<paint_preview::PaintPreviewTabService*>(
      GetInstance()->GetServiceForKey(key, true));
}

PaintPreviewTabServiceFactory::PaintPreviewTabServiceFactory()
    : SimpleKeyedServiceFactory("PaintPreviewTabService",
                                SimpleDependencyManager::GetInstance()) {}

PaintPreviewTabServiceFactory::~PaintPreviewTabServiceFactory() = default;

std::unique_ptr<KeyedService>
PaintPreviewTabServiceFactory::BuildServiceInstanceFor(
    SimpleFactoryKey* key) const {
  // Prevent this working off the record.
  if (key->IsOffTheRecord())
    return nullptr;

  // TODO(crbug.com/40122082): Inject a useful policy.
  return std::make_unique<paint_preview::PaintPreviewTabService>(
      std::make_unique<PaintPreviewTabServiceFileMixin>(key->GetPath(),
                                                        kFeatureDirname),
      nullptr, key->IsOffTheRecord());
}

SimpleFactoryKey* PaintPreviewTabServiceFactory::GetKeyToUse(
    SimpleFactoryKey* key) const {
  return key;
}

#if BUILDFLAG(IS_ANDROID)
base::android::ScopedJavaLocalRef<jobject>
JNI_PaintPreviewTabServiceFactory_GetServiceInstanceForCurrentProfile(
    JNIEnv* env) {
  ProfileKey* profile_key =
      ProfileManager::GetLastUsedProfile()->GetProfileKey();
  base::android::ScopedJavaGlobalRef<jobject> java_ref =
      PaintPreviewTabServiceFactory::GetServiceInstance(profile_key)
          ->GetJavaRef();
  return base::android::ScopedJavaLocalRef<jobject>(java_ref);
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace paint_preview
