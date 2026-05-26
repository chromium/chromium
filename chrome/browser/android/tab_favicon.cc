// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/tab_favicon.h"

#include "base/android/callback_android.h"
#include "base/functional/bind.h"
#include "chrome/browser/android/tab_android.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "content/public/browser/back_forward_transition_animation_manager.h"
#include "content/public/browser/web_contents.h"
#include "skia/ext/image_operations.h"
#include "skia/ext/skia_utils_base.h"
#include "third_party/blink/public/common/features_generated.h"
#include "ui/android/view_android.h"
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

using base::android::JavaRef;
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

TabFavicon* FromTabAndroid(TabAndroid* tab_android) {
  if (!tab_android) {
    return nullptr;
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  jlong native_ptr = Java_TabFavicon_getNativePtrForTab(env, tab_android);
  return reinterpret_cast<TabFavicon*>(native_ptr);
}

}  // namespace

// static
SkBitmap TabFavicon::GetBitmapForTab(TabAndroid* tab_android,
                                     bool allow_fallback) {
  if (!tab_android) {
    return SkBitmap();
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> bitmap =
      Java_TabFavicon_getBitmapWithFallback(env, tab_android, allow_fallback);
  if (bitmap.is_null()) {
    return SkBitmap();
  }
  return gfx::CreateSkBitmapFromJavaBitmap(gfx::JavaBitmap(bitmap));
}

// static
void TabFavicon::GetFaviconOrFallback(
    TabAndroid* tab_android,
    base::OnceCallback<void(const SkBitmap&)> callback) {
  if (!tab_android) {
    std::move(callback).Run(SkBitmap());
    return;
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_tab_favicon =
      Java_TabFavicon_from(env, tab_android);
  if (j_tab_favicon.is_null()) {
    std::move(callback).Run(SkBitmap());
    return;
  }
  auto j_callback = base::android::ToJniCallback(
      env, base::BindOnce(&TabFavicon::OnGetFaviconOrFallbackFinished,
                          std::move(callback)));

  Java_TabFavicon_getFaviconOrFallback(env, j_tab_favicon, j_callback);
}

// static
void TabFavicon::OnGetFaviconOrFallbackFinished(
    base::OnceCallback<void(const SkBitmap&)> callback,
    const base::android::JavaRef<jobject>& j_bitmap) {
  SkBitmap bitmap;
  if (!j_bitmap.is_null()) {
    bitmap = gfx::CreateSkBitmapFromJavaBitmap(gfx::JavaBitmap(j_bitmap));
  }

  std::move(callback).Run(bitmap);
}

// static
void TabFavicon::AddObserver(TabAndroid* tab_android, Observer* observer) {
  TabFavicon* tab_favicon = FromTabAndroid(tab_android);
  if (tab_favicon) {
    tab_favicon->observers_.AddObserver(observer);
  }
}

// static
void TabFavicon::RemoveObserver(TabAndroid* tab_android, Observer* observer) {
  TabFavicon* tab_favicon = FromTabAndroid(tab_android);
  if (tab_favicon) {
    tab_favicon->observers_.RemoveObserver(observer);
  }
}

// static
void TabFavicon::GetBitmapForTabOrFallback(
    TabAndroid* tab_android,
    base::OnceCallback<void(const SkBitmap&)> callback) {
  SkBitmap bitmap = GetBitmapForTab(tab_android, /*allow_fallback=*/false);
  if (!bitmap.empty()) {
    std::move(callback).Run(bitmap);
    return;
  }
  GetFaviconOrFallback(tab_android, std::move(callback));
}

TabFavicon::TabFavicon(JNIEnv* env,
                       TabAndroid* tab_android,
                       int navigation_transition_favicon_size)
    : navigation_transition_favicon_size_(navigation_transition_favicon_size),
      tab_android_(tab_android) {}

TabFavicon::~TabFavicon() {
  if (favicon_driver_) {
    favicon_driver_->RemoveObserver(this);
    favicon_driver_ = nullptr;
  }
}

void TabFavicon::SetWebContents(JNIEnv* env,
                                content::WebContents* web_contents) {
  if (favicon_driver_) {
    favicon_driver_->RemoveObserver(this);
  }
  active_web_contents_ = web_contents;
  favicon_driver_ =
      active_web_contents_
          ? favicon::ContentFaviconDriver::FromWebContents(active_web_contents_)
          : nullptr;
  if (favicon_driver_) {
    favicon_driver_->AddObserver(this);
  }
}

void TabFavicon::ResetWebContents(JNIEnv* env) {
  active_web_contents_ = nullptr;
  if (favicon_driver_) {
    favicon_driver_->RemoveObserver(this);
    favicon_driver_ = nullptr;
  }
}

void TabFavicon::OnDestroyed(JNIEnv* env) {
  delete this;
}

ScopedJavaLocalRef<jobject> TabFavicon::GetFavicon(JNIEnv* env) {
  ScopedJavaLocalRef<jobject> bitmap;

  if (!favicon_driver_ || !favicon_driver_->FaviconIsValid()) {
    return bitmap;
  }

  // Always return the default favicon in Android.
  SkBitmap favicon = favicon_driver_->GetFavicon().AsBitmap();
  if (!favicon.empty()) {
    float device_scale_factor = 1.0f;
    if (active_web_contents_ && active_web_contents_->GetNativeView()) {
      device_scale_factor =
          active_web_contents_->GetNativeView()->GetDipScale();
    } else {
      device_scale_factor =
          display::Screen::Get()->GetPrimaryDisplay().device_scale_factor();
    }

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
  if (favicon.empty()) {
    return;
  }

  // Notify C++ observers!
  for (auto& observer : observers_) {
    observer.OnFaviconUpdated(favicon);
  }

  CHECK(tab_android_);

  JNIEnv* env = base::android::AttachCurrentThread();

  auto new_width = image.Width();
  auto new_height = image.Height();
  if (static_cast<bool>(Java_TabFavicon_shouldUpdateFaviconForBrowserUi(
          env, tab_android_, new_width, new_height))) {
    Java_TabFavicon_onFaviconAvailable(
        env, tab_android_, gfx::ConvertToJavaBitmap(favicon), icon_url);
  }
  if (content::BackForwardTransitionAnimationManager::
          ShouldAnimateBackForwardTransitions()) {
    CHECK(active_web_contents_);
    if (static_cast<bool>(
            Java_TabFavicon_shouldUpdateFaviconForNavigationTransitions(
                env, tab_android_, new_width, new_height))) {
      SkBitmap scaled =
          RescaleSkBitmap(favicon, navigation_transition_favicon_size_);
      auto* manager =
          active_web_contents_->GetBackForwardTransitionAnimationManager();
      CHECK(manager);
      manager->SetFavicon(std::move(scaled));
    }
  }
}

static int64_t JNI_TabFavicon_Init(JNIEnv* env,
                                   TabAndroid* tab_android,
                                   int navigation_transition_favicon_size) {
  CHECK(tab_android);
  return reinterpret_cast<intptr_t>(
      new TabFavicon(env, tab_android, navigation_transition_favicon_size));
}

DEFINE_JNI(TabFavicon)
