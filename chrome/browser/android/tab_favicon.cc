// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/tab_favicon.h"

#include "chrome/android/chrome_jni_headers/TabFavicon_jni.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "content/public/browser/web_contents.h"
#include "skia/ext/image_operations.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image_skia.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

TabFavicon::TabFavicon(JNIEnv* env, const JavaParamRef<jobject>& obj)
    : jobj_(env, obj) {}

TabFavicon::~TabFavicon() = default;

void TabFavicon::SetWebContents(JNIEnv* env,
                                const JavaParamRef<jobject>& obj,
                                const JavaParamRef<jobject>& jweb_contents) {
  favicon_driver_ = favicon::ContentFaviconDriver::FromWebContents(
      content::WebContents::FromJavaWebContents(jweb_contents));
  if (favicon_driver_)
    favicon_driver_->AddObserver(this);
}

void TabFavicon::ResetWebContents(JNIEnv* env,
                                  const JavaParamRef<jobject>& obj) {
  if (favicon_driver_) {
    favicon_driver_->RemoveObserver(this);
    favicon_driver_ = nullptr;
  }
}

void TabFavicon::OnDestroyed(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  delete this;
}

ScopedJavaLocalRef<jobject> TabFavicon::GetFavicon(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  ScopedJavaLocalRef<jobject> bitmap;

  if (!favicon_driver_)
    return bitmap;

  // Always return the default favicon in Android.
  SkBitmap favicon = favicon_driver_->GetFavicon().AsBitmap();
  if (!favicon.empty()) {
    const float device_scale_factor =
        display::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor();
    int target_size_dip = device_scale_factor * gfx::kFaviconSize;
    if (favicon.width() != target_size_dip ||
        favicon.height() != target_size_dip) {
      favicon = skia::ImageOperations::Resize(
          favicon, skia::ImageOperations::RESIZE_BEST, target_size_dip,
          target_size_dip);
    }

    bitmap = gfx::ConvertToJavaBitmap(&favicon);
  }
  return bitmap;
}

void TabFavicon::OnFaviconUpdated(favicon::FaviconDriver* favicon_driver,
                                  NotificationIconType notification_icon_type,
                                  const GURL& icon_url,
                                  bool icon_url_changed,
                                  const gfx::Image& image) {
  if (notification_icon_type != NON_TOUCH_LARGEST &&
      notification_icon_type != TOUCH_LARGEST) {
    return;
  }

  SkBitmap favicon = image.AsImageSkia().GetRepresentation(1.0f).GetBitmap();
  if (favicon.empty())
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_TabFavicon_onFaviconAvailable(env, jobj_,
                                     gfx::ConvertToJavaBitmap(&favicon));
}

static jlong JNI_TabFavicon_Init(JNIEnv* env,
                                 const JavaParamRef<jobject>& obj) {
  return reinterpret_cast<intptr_t>(new TabFavicon(env, obj));
}
