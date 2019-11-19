// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/large_icon_bridge.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/android/chrome_jni_headers/LargeIconBridge_jni.h"
#include "chrome/browser/favicon/large_icon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/favicon_base/fallback_icon_style.h"
#include "components/favicon_base/favicon_types.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/codec/png_codec.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace {

void OnLargeIconAvailable(const JavaRef<jobject>& j_callback,
                          const favicon_base::LargeIconResult& result) {
  JNIEnv* env = AttachCurrentThread();

  // Convert the result to a Java Bitmap.
  SkBitmap bitmap;
  ScopedJavaLocalRef<jobject> j_bitmap;
  if (result.bitmap.is_valid()) {
    gfx::PNGCodec::Decode(result.bitmap.bitmap_data->front(),
                          result.bitmap.bitmap_data->size(), &bitmap);
    if (!bitmap.isNull())
      j_bitmap = gfx::ConvertToJavaBitmap(&bitmap);
  }

  favicon_base::FallbackIconStyle fallback;
  if (result.fallback_icon_style)
    fallback = *result.fallback_icon_style;

  Java_LargeIconCallback_onLargeIconAvailable(
      env, j_callback, j_bitmap, fallback.background_color,
      fallback.is_default_background_color,
      static_cast<int>(result.bitmap.icon_type));
}

}  // namespace

static jlong JNI_LargeIconBridge_Init(JNIEnv* env) {
  return reinterpret_cast<intptr_t>(new LargeIconBridge());
}

LargeIconBridge::LargeIconBridge() {}

LargeIconBridge::~LargeIconBridge() {}

void LargeIconBridge::Destroy(JNIEnv* env) {
  delete this;
}

jboolean LargeIconBridge::GetLargeIconForURL(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile,
    const JavaParamRef<jstring>& j_page_url,
    jint min_source_size_px,
    const JavaParamRef<jobject>& j_callback) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  if (!profile)
    return false;

  favicon::LargeIconService* large_icon_service =
      LargeIconServiceFactory::GetForBrowserContext(profile);
  if (!large_icon_service)
    return false;

  favicon_base::LargeIconCallback callback_runner = base::BindOnce(
      &OnLargeIconAvailable, ScopedJavaGlobalRef<jobject>(env, j_callback));

  // Use desired_size = 0 for getting the icon from the cache (so that
  // the icon is not poorly rescaled by LargeIconService).
  large_icon_service->GetLargeIconRawBitmapOrFallbackStyleForPageUrl(
      GURL(ConvertJavaStringToUTF16(env, j_page_url)), min_source_size_px,
      /*desired_size_in_pixel=*/0, std::move(callback_runner),
      &cancelable_task_tracker_);

  return true;
}
