// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/prefs/cross_device_pref_tracker/cross_device_pref_tracker_factory.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/prefs/cross_device_pref_tracker/chrome_cross_device_pref_provider.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/sync_preferences/cross_device_pref_tracker/cross_device_pref_tracker_impl.h"
#include "components/sync_preferences/features.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/sync/android/jni_headers/CrossDevicePrefTrackerFactory_jni.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace {

// Helper function to determine ProfileSelections based on the feature flag.
ProfileSelections BuildCrossDevicePrefTrackerProfileSelections() {
  if (!base::FeatureList::IsEnabled(
          sync_preferences::features::kEnableCrossDevicePrefTracker)) {
    return ProfileSelections::BuildNoProfilesSelected();
  }

  return ProfileSelections::Builder()
      .WithRegular(ProfileSelection::kOriginalOnly)
      .WithAshInternals(ProfileSelection::kNone)
      .Build();
}

}  // namespace

CrossDevicePrefTrackerFactory::CrossDevicePrefTrackerFactory()
    : ProfileKeyedServiceFactory(
          "CrossDevicePrefTracker",
          BuildCrossDevicePrefTrackerProfileSelections()) {
  DependsOn(DeviceInfoSyncServiceFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
}

CrossDevicePrefTrackerFactory::~CrossDevicePrefTrackerFactory() = default;

// static
sync_preferences::CrossDevicePrefTracker*
CrossDevicePrefTrackerFactory::GetForProfile(Profile* profile) {
  return static_cast<sync_preferences::CrossDevicePrefTracker*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
CrossDevicePrefTrackerFactory* CrossDevicePrefTrackerFactory::GetInstance() {
  static base::NoDestructor<CrossDevicePrefTrackerFactory> instance;
  return instance.get();
}

std::unique_ptr<KeyedService>
CrossDevicePrefTrackerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  auto pref_provider = std::make_unique<ChromeCrossDevicePrefProvider>();
  return std::make_unique<sync_preferences::CrossDevicePrefTrackerImpl>(
      profile->GetPrefs(), g_browser_process->local_state(),
      DeviceInfoSyncServiceFactory::GetForProfile(profile),
      SyncServiceFactory::GetForProfile(profile), std::move(pref_provider));
}

#if BUILDFLAG(IS_ANDROID)
static base::android::ScopedJavaLocalRef<jobject>
JNI_CrossDevicePrefTrackerFactory_GetForProfile(JNIEnv* env, Profile* profile) {
  DCHECK(profile);

  sync_preferences::CrossDevicePrefTracker* pref_tracker =
      CrossDevicePrefTrackerFactory::GetForProfile(profile);
  if (!pref_tracker) {
    return base::android::ScopedJavaLocalRef<jobject>();
  }
  return pref_tracker->GetJavaObject();
}
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
DEFINE_JNI(CrossDevicePrefTrackerFactory)
#endif
