// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "chrome/android/chrome_jni_headers/TileProviderFactory_jni.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/query_tiles/tile_service_factory.h"
#include "components/query_tiles/android/tile_provider_bridge.h"

// Takes a Java Profile and returns a Java TileProvider.
static base::android::ScopedJavaLocalRef<jobject>
JNI_TileProviderFactory_GetForProfile(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_profile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  ProfileKey* profile_key = profile->GetProfileKey();

  // Return null if there is no reasonable context for the provided Java
  // profile.
  if (profile_key == nullptr)
    return base::android::ScopedJavaLocalRef<jobject>();

  query_tiles::TileService* tile_service =
      query_tiles::TileServiceFactory::GetInstance()->GetForKey(profile_key);
  return query_tiles::TileProviderBridge::GetBridgeForTileService(tile_service);
}
