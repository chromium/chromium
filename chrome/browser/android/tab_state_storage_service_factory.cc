// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/tab_state_storage_service_factory.h"

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab/jni_headers/TabStateStorageServiceFactory_jni.h"
#include "chrome/browser/tab/restore_entity_tracker.h"
#include "chrome/browser/tab/tab_state_storage_service.h"
#include "chrome/browser/tab/tab_storage_packager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/browser_context.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/restore_entity_tracker_android.h"
#include "chrome/browser/android/tab_android_conversions.h"
#include "chrome/browser/android/tab_storage_packager_android.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace tabs {

namespace {

TabCanonicalizer GetTabCanonicalizer() {
#if BUILDFLAG(IS_ANDROID)
  return base::BindRepeating(
      [](const TabInterface* tab) -> const TabInterface* {
        return ToTabAndroidChecked(tab);
      });
#else
  return base::BindRepeating([](const TabInterface* tab) { return tab; });
#endif  // !BUILDFLAG(IS_ANDROID)
}

RestoreEntityTrackerFactory GetRestoreEntityTrackerFactory() {
#if BUILDFLAG(IS_ANDROID)
  return base::BindRepeating(
      [](OnTabAssociation on_tab_associated,
         OnCollectionAssociation on_collection_associated)
          -> std::unique_ptr<RestoreEntityTracker> {
        return std::make_unique<RestoreEntityTrackerAndroid>(
            on_tab_associated, on_collection_associated);
      });
#else
  return base::BindRepeating(
      [](OnTabAssociation,
         OnCollectionAssociation) -> std::unique_ptr<RestoreEntityTracker> {
        return nullptr;
      });
#endif  // !BUILDFLAG(IS_ANDROID)
}

}  // namespace

static base::android::ScopedJavaLocalRef<jobject>
JNI_TabStateStorageServiceFactory_GetForProfile(JNIEnv* env, Profile* profile) {
  CHECK(profile);
  TabStateStorageService* service =
      TabStateStorageServiceFactory::GetForProfile(profile);
  CHECK(service);
  return service->GetJavaObject(service);
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

  Profile* profile = static_cast<Profile*>(context);
  std::unique_ptr<TabStoragePackager> packager;
#if BUILDFLAG(IS_ANDROID)
  packager = std::make_unique<TabStoragePackagerAndroid>(profile);
#endif
  return std::make_unique<TabStateStorageService>(
      profile->GetPath(), std::move(packager), GetTabCanonicalizer(),
      GetRestoreEntityTrackerFactory());
}

}  // namespace tabs

DEFINE_JNI(TabStateStorageServiceFactory)
