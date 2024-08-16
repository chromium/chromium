// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/tab_favicon.h"

#include "components/favicon/content/content_favicon_driver.h"
#include "content/public/browser/back_forward_transition_animation_manager.h"
#include "content/public/browser/web_contents.h"
#include "skia/ext/image_operations.h"
#include "third_party/blink/public/common/features_generated.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "url/android/gurl_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/TabFavicon_jni.h"

namespace {

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

SkBitmap RescaleSkBitmap(const SkBitmap& original, int new_size_dip) {
  SkImageInfo scaledInfo = original.info().makeWH(new_size_dip, new_size_dip);
  SkBitmap resized;
  resized.allocPixels(scaledInfo);
  bool success = original.pixmap().scalePixels(
      resized.pixmap(), SkSamplingOptions(SkFilterMode::kLinear));
  CHECK(success);
  resized.setImmutable();
  return resized;
}

}  // namespace

TabFavicon::TabFavicon(JNIEnv* env,
                       const JavaParamRef<jobject>& obj,
                       int navigation_transition_favicon_size)
    : navigation_transition_favicon_size_(navigation_transition_favicon_size),
      jobj_(env, obj) {}

TabFavicon::~TabFavicon() = default;

void TabFavicon::SetWebContents(JNIEnv* env,
                                const JavaParamRef<jobject>& obj,
                                const JavaParamRef<jobject>& jweb_contents) {
  active_web_contents_ =
      content::WebContents::FromJavaWebContents(jweb_contents);
  favicon_driver_ =
      favicon::ContentFaviconDriver::FromWebContents(active_web_contents_);
  if (favicon_driver_)
    favicon_driver_->AddObserver(this);
}

void TabFavicon::ResetWebContents(JNIEnv* env,
                                  const JavaParamRef<jobject>& obj) {
  active_web_contents_ = nullptr;
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

  if (!favicon_driver_ || !favicon_driver_->FaviconIsValid()) {
    return bitmap;
  }

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

    bitmap = gfx::ConvertToJavaBitmap(favicon);
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

  auto new_width = image.Width();
  auto new_height = image.Height();
  if (static_cast<bool>(Java_TabFavicon_shouldUpdateFaviconForBrowserUI(
          env, jobj_, new_width, new_height))) {
    ScopedJavaLocalRef<jobject> j_icon_url =
        url::GURLAndroid::FromNativeGURL(env, icon_url);
    Java_TabFavicon_onFaviconAvailable(
        env, jobj_, gfx::ConvertToJavaBitmap(favicon), j_icon_url);
  }
  if (base::FeatureList::IsEnabled(blink::features::kBackForwardTransitions)) {
    CHECK(active_web_contents_);
    if (static_cast<bool>(
            Java_TabFavicon_shouldUpdateFaviconForNavigationTransitions(
                env, jobj_, new_width, new_height))) {
      SkBitmap scaled =
          RescaleSkBitmap(favicon, navigation_transition_favicon_size_);
      auto* manager =
          active_web_contents_->GetBackForwardTransitionAnimationManager();
      CHECK(manager);
      manager->SetFavicon(std::move(scaled));
    }
  }
}

static jlong JNI_TabFavicon_Init(JNIEnv* env,
                                 const JavaParamRef<jobject>& obj,
                                 int navigation_transition_favicon_size) {
  return reinterpret_cast<intptr_t>(
      new TabFavicon(env, obj, navigation_transition_favicon_size));
}
