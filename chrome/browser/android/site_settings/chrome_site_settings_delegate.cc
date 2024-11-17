// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/containers/contains.h"
#include "chrome/browser/file_system_access/chrome_file_system_access_permission_context.h"
#include "chrome/browser/file_system_access/file_system_access_permission_context_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "url/gurl.h"
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

static jni_zero::ScopedJavaLocalRef<jobjectArray>
JNI_ChromeSiteSettingsDelegate_GetFileSystemAccessGrants(
    JNIEnv* env,
    const jni_zero::JavaParamRef<jobject>& j_profile,
    std::string& origin) {
  Profile* profile = Profile::FromJavaObject(j_profile);
  std::vector<std::string> paths;
  std::vector<std::string> display_names;
  auto* context =
      FileSystemAccessPermissionContextFactory::GetForProfileIfExists(profile);
  if (context) {
    ChromeFileSystemAccessPermissionContext::Grants grants =
        context->ConvertObjectsToGrants(
            context->GetGrantedObjects(url::Origin::Create(GURL(origin))));
    for (const content::PathInfo& grant : grants.file_write_grants) {
      paths.push_back(grant.path.value());
      display_names.push_back(grant.display_name);
    }
    for (const content::PathInfo& grant : grants.directory_write_grants) {
      paths.push_back(grant.path.value());
      display_names.push_back(grant.display_name);
    }
    for (const content::PathInfo& grant : grants.file_read_grants) {
      if (!base::Contains(grants.file_write_grants, grant)) {
        paths.push_back(grant.path.value());
        display_names.push_back(grant.display_name);
      }
    }
    for (const content::PathInfo& grant : grants.directory_read_grants) {
      if (!base::Contains(grants.directory_write_grants, grant)) {
        paths.push_back(grant.path.value());
        display_names.push_back(grant.display_name);
      }
    }
  }
  return base::android::ToJavaArrayOfStringArray(
      env, std::vector<std::vector<std::string>>(
               {std::move(paths), std::move(display_names)}));
}

static void JNI_ChromeSiteSettingsDelegate_RevokeFileSystemAccessGrant(
    JNIEnv* env,
    const jni_zero::JavaParamRef<jobject>& j_profile,
    std::string& origin,
    std::string& file) {
  Profile* profile = Profile::FromJavaObject(j_profile);
  auto* context =
      FileSystemAccessPermissionContextFactory::GetForProfileIfExists(profile);
  if (context) {
    context->RevokeGrant(url::Origin::Create(GURL(origin)),
                         base::FilePath(file));
  }
}
