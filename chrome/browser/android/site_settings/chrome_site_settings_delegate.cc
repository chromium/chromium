// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/browser/file_system_access/chrome_file_system_access_permission_context.h"
#include "chrome/browser/file_system_access/file_system_access_permission_context_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "url/origin.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/ChromeSiteSettingsDelegate_jni.h"

static std::vector<std::string>
JNI_ChromeSiteSettingsDelegate_GetOriginsWithFileSystemAccessGrants(
    JNIEnv* env,
    const jni_zero::JavaParamRef<jobject>& j_profile) {
  Profile* profile = Profile::FromJavaObject(j_profile);
  std::vector<std::string> result;
  auto* context =
      FileSystemAccessPermissionContextFactory::GetForProfileIfExists(profile);
  if (context) {
    std::set<url::Origin> origins = context->GetOriginsWithGrants();
    for (const auto& origin : origins) {
      result.push_back(origin.Serialize());
    }
  }
  return result;
}
