// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/check_op.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/platform_notification_context.h"
#include "content/public/browser/storage_partition.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/NotificationSuspender_jni.h"

using base::android::AppendJavaStringArrayToStringVector;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using content::BrowserContext;
using content::NotificationResourceData;
using content::PlatformNotificationContext;
using jni_zero::AttachCurrentThread;

namespace {

SkBitmap ExtractImage(JNIEnv* env,
                      const JavaParamRef<jobjectArray>& j_resources,
                      int index) {
  ScopedJavaLocalRef<jobject> j_image(
      env, env->GetObjectArrayElement(j_resources, index));
  return j_image.is_null()
             ? SkBitmap()
             : CreateSkBitmapFromJavaBitmap(gfx::JavaBitmap(j_image));
}

std::vector<blink::NotificationResources> ParseResources(
    JNIEnv* env,
    const JavaParamRef<jobjectArray>& j_resources) {
  // Resources is an array of bitmaps with the following order:
  // [icon, badge, image, icon, badge, image, ...]
  int resource_count = env->GetArrayLength(j_resources);
  DCHECK(resource_count % 3 == 0);

  std::vector<blink::NotificationResources> resources;
  for (int i = 0; i < resource_count; i += 3) {
    blink::NotificationResources res;
    res.notification_icon = ExtractImage(env, j_resources, i + 0);
    res.badge = ExtractImage(env, j_resources, i + 1);
    res.image = ExtractImage(env, j_resources, i + 2);
    resources.emplace_back(std::move(res));
  }
  return resources;
}

PlatformNotificationContext* GetContext(Profile* profile, const GURL& origin) {
  auto* partition = profile->GetStoragePartitionForUrl(origin);
  auto* context = partition->GetPlatformNotificationContext();
  DCHECK(context);
  return context;
}

}  // namespace

// Stores the given |j_resources| to be displayed later again. Note that
// |j_resources| is expected to have 3 entries (icon, badge, image in that
// order) for each notification id in |j_notification_ids|. If a notification
// does not have a particular resource, pass null instead. |j_origins| must be
// the same size as |j_notification_ids|.
static void JNI_NotificationSuspender_StoreNotificationResources(
    JNIEnv* env,
    Profile* profile,
    const JavaParamRef<jobjectArray>& j_notification_ids,
    const JavaParamRef<jobjectArray>& j_origins,
    const JavaParamRef<jobjectArray>& j_resources) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile);

  std::vector<std::string> id_strings;
  AppendJavaStringArrayToStringVector(env, j_notification_ids, &id_strings);
  std::vector<std::string> origin_strings;
  AppendJavaStringArrayToStringVector(env, j_origins, &origin_strings);
  std::vector<blink::NotificationResources> resources =
      ParseResources(env, j_resources);

  DCHECK(id_strings.size() == origin_strings.size());
  DCHECK(id_strings.size() == resources.size());

  // Group resources by context.
  std::map<PlatformNotificationContext*, std::vector<NotificationResourceData>>
      resources_by_context;
  for (size_t i = 0; i < id_strings.size(); ++i) {
    GURL origin(std::move(origin_strings[i]));
    if (!origin.is_valid() || !origin.SchemeIsHTTPOrHTTPS())
      continue;
    PlatformNotificationContext* context = GetContext(profile, origin);
    resources_by_context[context].emplace_back(NotificationResourceData{
        std::move(id_strings[i]), std::move(origin), std::move(resources[i])});
  }

  // Store resources in each context.
  for (auto& entry : resources_by_context) {
    entry.first->WriteNotificationResources(std::move(entry.second),
                                            base::DoNothing());
  }
}

// ReDisplays all notifications with stored resources for all |j_origins|.
static void JNI_NotificationSuspender_ReDisplayNotifications(
    JNIEnv* env,
    Profile* profile,
    std::vector<std::string>& origin_strings) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile);

  // Group origins by context.
  std::map<PlatformNotificationContext*, std::vector<GURL>> origins_by_context;
  for (std::string& origin_string : origin_strings) {
    GURL origin(std::move(origin_string));
    if (!origin.is_valid() || !origin.SchemeIsHTTPOrHTTPS())
      continue;
    origins_by_context[GetContext(profile, origin)].emplace_back(
        std::move(origin));
  }

  // ReDisplay notifications from each context.
  for (auto& entry : origins_by_context) {
    entry.first->ReDisplayNotifications(std::move(entry.second),
                                        base::DoNothing());
  }
}
