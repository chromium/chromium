// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/favicon_helper.h"

#include <jni.h>
#include <stddef.h>

#include <memory>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/compose_bitmaps_helper.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/favicon/history_ui_favicon_request_handler_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon/core/favicon_util.h"
#include "components/favicon/core/history_ui_favicon_request_handler.h"
#include "components/favicon_base/favicon_util.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "url/android/gurl_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/favicon/jni_headers/FaviconHelper_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

static jlong JNI_FaviconHelper_Init(JNIEnv* env) {
  return reinterpret_cast<intptr_t>(new FaviconHelper());
}

FaviconHelper::FaviconHelper() {
  cancelable_task_tracker_ = std::make_unique<base::CancelableTaskTracker>();
}

void FaviconHelper::Destroy(JNIEnv* env) {
  delete this;
}

jboolean FaviconHelper::GetLocalFaviconImageForURL(
    JNIEnv* env,
    Profile* profile,
    GURL& page_url,
    jint j_desired_size_in_pixel,
    const JavaParamRef<jobject>& j_favicon_image_callback) {
  DCHECK(profile);
  if (!profile) {
    return false;
  }

  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  DCHECK(favicon_service);
  if (!favicon_service) {
    return false;
  }

  favicon_base::FaviconRawBitmapCallback callback_runner =
      base::BindOnce(&FaviconHelper::OnFaviconBitmapResultAvailable,
                     weak_ptr_factory_.GetWeakPtr(),
                     ScopedJavaGlobalRef<jobject>(j_favicon_image_callback));

  GetLocalFaviconImageForURLInternal(favicon_service, page_url,
                                     static_cast<int>(j_desired_size_in_pixel),
                                     std::move(callback_runner));

  return true;
}

void FaviconHelper::GetLocalFaviconImageForURLInternal(
    favicon::FaviconService* favicon_service,
    GURL url,
    int desired_size_in_pixel,
    favicon_base::FaviconRawBitmapCallback callback_runner) {
  DCHECK(favicon_service);
  if (!favicon_service) {
    return;
  }

  // |j_page_url| is an origin, and it may not have had a favicon associated
  // with it. A trickier case is when |j_page_url| only has domain-scoped
  // cookies, but visitors are redirected to HTTPS on visiting. Then
  // |j_page_url| defaults to a HTTP scheme, but the favicon will be associated
  // with the HTTPS URL and hence won't be found if we include the scheme in the
  // lookup. Set |fallback_to_host|=true so the favicon database will fall back
  // to matching only the hostname to have the best chance of finding a favicon.
  const bool fallback_to_host = true;
  favicon_service->GetRawFaviconForPageURL(
      url,
      {favicon_base::IconType::kFavicon, favicon_base::IconType::kTouchIcon,
       favicon_base::IconType::kTouchPrecomposedIcon,
       favicon_base::IconType::kWebManifestIcon},
      desired_size_in_pixel, fallback_to_host, std::move(callback_runner),
      cancelable_task_tracker_.get());
}

jboolean FaviconHelper::GetForeignFaviconImageForURL(
    JNIEnv* env,
    Profile* profile,
    GURL& page_url,
    jint j_desired_size_in_pixel,
    const base::android::JavaParamRef<jobject>& j_favicon_image_callback) {
  if (!profile) {
    return false;
  }

  favicon::HistoryUiFaviconRequestHandler* history_ui_favicon_request_handler =
      HistoryUiFaviconRequestHandlerFactory::GetForBrowserContext(profile);
  // Can be null in tests.
  if (!history_ui_favicon_request_handler) {
    return false;
  }
  history_ui_favicon_request_handler->GetRawFaviconForPageURL(
      page_url, static_cast<int>(j_desired_size_in_pixel),
      /*fallback_to_host=*/true,
      base::BindOnce(&FaviconHelper::OnFaviconBitmapResultAvailable,
                     weak_ptr_factory_.GetWeakPtr(),
                     ScopedJavaGlobalRef<jobject>(j_favicon_image_callback)));
  return true;
}

FaviconHelper::~FaviconHelper() = default;

void FaviconHelper::OnFaviconBitmapResultAvailable(
    const JavaRef<jobject>& j_favicon_image_callback,
    const favicon_base::FaviconRawBitmapResult& result) {
  JNIEnv* env = AttachCurrentThread();

  // Convert favicon_image_result to java objects.
  ScopedJavaLocalRef<jobject> j_favicon_bitmap;
  if (result.is_valid()) {
    SkBitmap favicon_bitmap = gfx::PNGCodec::Decode(*result.bitmap_data);
    if (!favicon_bitmap.isNull()) {
      j_favicon_bitmap = gfx::ConvertToJavaBitmap(favicon_bitmap);
    }
  }

  // Call java side OnFaviconBitmapResultAvailable method.
  Java_FaviconImageCallback_onFaviconAvailable(
      env, j_favicon_image_callback, j_favicon_bitmap, result.icon_url);
}

DEFINE_JNI(FaviconHelper)
