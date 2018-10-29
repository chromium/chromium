// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/android/explore_sites/explore_sites_bridge.h"
#include "chrome/browser/android/explore_sites/explore_sites_feature.h"
#include "chrome/browser/android/explore_sites/explore_sites_service.h"
#include "chrome/browser/android/explore_sites/explore_sites_service_factory.h"
#include "chrome/browser/android/explore_sites/explore_sites_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "jni/ExploreSitesBridge_jni.h"
#include "jni/ExploreSitesCategory_jni.h"
#include "jni/ExploreSitesSite_jni.h"
#include "ui/gfx/android/java_bitmap.h"

namespace explore_sites {
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace {

void CatalogReady(ScopedJavaGlobalRef<jobject>(j_result_obj),
                  ScopedJavaGlobalRef<jobject>(j_callback_obj),
                  GetCatalogStatus status,
                  std::unique_ptr<std::vector<ExploreSitesCategory>> result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  // TODO(dewittj): Pass along the status to Java land.
  if (status == GetCatalogStatus::kNoCatalog) {
    // Don't fill in the result, just return empty.
    base::android::RunObjectCallbackAndroid(j_callback_obj, j_result_obj);
    return;
  }

  if (!result) {
    DLOG(ERROR) << "Unable to fetch the ExploreSites catalog!";
    base::android::RunObjectCallbackAndroid(j_callback_obj, nullptr);
    return;
  }

  for (auto& category : *result) {
    ScopedJavaLocalRef<jobject> j_category =
        Java_ExploreSitesCategory_createAndAppendToList(
            env, category.category_id, category.category_type,
            ConvertUTF8ToJavaString(env, category.label), j_result_obj);
    for (auto& site : category.sites) {
      Java_ExploreSitesSite_createSiteInCategory(
          env, site.site_id, ConvertUTF8ToJavaString(env, site.title),
          ConvertUTF8ToJavaString(env, site.url.spec()), j_category);
    }
  }
  base::android::RunObjectCallbackAndroid(j_callback_obj, j_result_obj);
}

void ImageReady(ScopedJavaGlobalRef<jobject>(j_callback_obj),
                std::unique_ptr<SkBitmap> bitmap) {
  if (!bitmap || bitmap->isNull()) {
    DVLOG(1) << "Site icon is empty.";
    base::android::RunObjectCallbackAndroid(j_callback_obj, nullptr);
    return;
  }

  ScopedJavaLocalRef<jobject> j_bitmap = gfx::ConvertToJavaBitmap(bitmap.get());

  base::android::RunObjectCallbackAndroid(j_callback_obj, j_bitmap);
}

void UpdateCatalogDone(ScopedJavaGlobalRef<jobject>(j_callback_obj),
                       bool result) {
  base::android::RunBooleanCallbackAndroid(j_callback_obj, result);
}

}  // namespace

// static
void JNI_ExploreSitesBridge_GetEspCatalog(
    JNIEnv* env,
    const JavaParamRef<jclass>& j_caller,
    const JavaParamRef<jobject>& j_profile,
    const JavaParamRef<jobject>& j_result_obj,
    const JavaParamRef<jobject>& j_callback_obj) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  DCHECK(profile);

  ExploreSitesService* service =
      ExploreSitesServiceFactory::GetForBrowserContext(profile);
  if (!service) {
    DLOG(ERROR) << "Unable to create the ExploreSitesService!";
    base::android::RunObjectCallbackAndroid(j_callback_obj, nullptr);
    return;
  }

  service->GetCatalog(
      base::BindOnce(&CatalogReady, ScopedJavaGlobalRef<jobject>(j_result_obj),
                     ScopedJavaGlobalRef<jobject>(j_callback_obj)));
}

// static
jint JNI_ExploreSitesBridge_GetVariation(JNIEnv* env,
                                         const JavaParamRef<jclass>& j_caller) {
  return static_cast<jint>(
      chrome::android::explore_sites::GetExploreSitesVariation());
}

// static
void JNI_ExploreSitesBridge_GetIcon(
    JNIEnv* env,
    const JavaParamRef<jclass>& j_caller,
    const JavaParamRef<jobject>& j_profile,
    const jint j_site_id,
    const JavaParamRef<jobject>& j_callback_obj) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  DCHECK(profile);

  ExploreSitesService* service =
      ExploreSitesServiceFactory::GetForBrowserContext(profile);
  if (!service) {
    DLOG(ERROR) << "Unable to create the ExploreSitesService!";
    base::android::RunObjectCallbackAndroid(j_callback_obj, nullptr);
    return;
  }
  int site_id = static_cast<int>(j_site_id);

  service->GetSiteImage(
      site_id, base::BindOnce(&ImageReady,
                              ScopedJavaGlobalRef<jobject>(j_callback_obj)));
}

void JNI_ExploreSitesBridge_UpdateCatalogFromNetwork(
    JNIEnv* env,
    const JavaParamRef<jclass>& j_caller,
    const JavaParamRef<jobject>& j_profile,
    jboolean is_immediate_fetch,
    const JavaParamRef<jobject>& j_callback_obj) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  DCHECK(profile);

  ExploreSitesService* service =
      ExploreSitesServiceFactory::GetForBrowserContext(profile);
  if (!service) {
    DLOG(ERROR) << "Unable to create the ExploreSitesService!";
    base::android::RunBooleanCallbackAndroid(j_callback_obj, false);
    return;
  }

  // TODO(petewil): It might be better to move the PrefService work inside
  // ExploreSitesService.
  std::string accept_languages;
  PrefService* pref_service = profile->GetPrefs();
  if (pref_service != nullptr) {
    accept_languages = pref_service->GetString(prefs::kAcceptLanguages);
  }

  service->UpdateCatalogFromNetwork(
      static_cast<bool>(is_immediate_fetch), accept_languages,
      base::BindOnce(&UpdateCatalogDone,
                     ScopedJavaGlobalRef<jobject>(j_callback_obj)));
}

void JNI_ExploreSitesBridge_BlacklistSite(
    JNIEnv* env,
    const JavaParamRef<jclass>& j_caller,
    const JavaParamRef<jobject>& j_profile,
    const JavaParamRef<jstring>& j_url) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  std::string url = ConvertJavaStringToUTF8(env, j_url);
  ExploreSitesService* service =
      ExploreSitesServiceFactory::GetForBrowserContext(profile);
  if (!service) {
    DLOG(ERROR) << "Unable to create the ExploreSitesService!";
    return;
  }

  service->BlacklistSite(url);
}

// static
void ExploreSitesBridge::ScheduleDailyTask() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ExploreSitesBridge_scheduleDailyTask(env);
}

float ExploreSitesBridge::GetScaleFactorFromDevice() {
  JNIEnv* env = base::android::AttachCurrentThread();
  // Get scale factor from Java as a float.
  return Java_ExploreSitesBridge_getScaleFactorFromDevice(env);
}

// static
void JNI_ExploreSitesBridge_GetCategoryImage(
    JNIEnv* env,
    const JavaParamRef<jclass>& j_caller,
    const JavaParamRef<jobject>& j_profile,
    const jint j_category_id,
    const jint j_pixel_size,
    const JavaParamRef<jobject>& j_callback_obj) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  DCHECK(profile);

  ExploreSitesService* service =
      ExploreSitesServiceFactory::GetForBrowserContext(profile);
  if (!service) {
    DLOG(ERROR) << "Unable to create the ExploreSitesService!";
    base::android::RunBooleanCallbackAndroid(j_callback_obj, false);
    return;
  }

  service->GetCategoryImage(
      j_category_id, j_pixel_size,
      base::BindOnce(&ImageReady,
                     ScopedJavaGlobalRef<jobject>(j_callback_obj)));
}
}  // namespace explore_sites
