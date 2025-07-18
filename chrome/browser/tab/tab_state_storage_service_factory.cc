// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/tab_state_storage_service_factory.h"

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab/jni_headers/TabStateStorageServiceFactory_jni.h"

namespace tabs {

static base::android::ScopedJavaLocalRef<jobject>
JNI_TabStateStorageServiceFactory_GetForProfile(JNIEnv* env, Profile* profile) {
  CHECK(profile);
  TabStateStorageService* service =
      TabStateStorageServiceFactory::GetForProfile(profile);
  CHECK(service);
  return service->GetJavaObject();
}

// static
TabStateStorageServiceFactory* TabStateStorageServiceFactory::GetInstance() {
  static base::NoDestructor<TabStateStorageServiceFactory> instance;
  return instance.get();
}

// static
TabStateStorageService* TabStateStorageServiceFactory::GetForProfile(
    Profile* profile) {
  CHECK(profile);
  return static_cast<TabStateStorageService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

TabStateStorageServiceFactory::TabStateStorageServiceFactory()
    : ProfileKeyedServiceFactory(
          "TabStateStorageService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {}

TabStateStorageServiceFactory::~TabStateStorageServiceFactory() = default;

std::unique_ptr<KeyedService>
TabStateStorageServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  DCHECK(context);

  if (!base::FeatureList::IsEnabled(
          chrome::android::kTabStorageSqlitePrototype)) {
    return nullptr;
  }

  Profile* profile = static_cast<Profile*>(context);
  std::unique_ptr<TabStateStorageBackend> tab_backend =
      std::make_unique<TabStateStorageBackend>(profile->GetPath());
  return std::make_unique<TabStateStorageService>(std::move(tab_backend));
}

}  // namespace tabs
