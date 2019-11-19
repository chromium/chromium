// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/favicon_helper.h"

#include <jni.h>
#include <stddef.h>

#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/android/chrome_jni_headers/FaviconHelper_jni.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/favicon/history_ui_favicon_request_handler_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon/core/favicon_util.h"
#include "components/favicon/core/history_ui_favicon_request_handler.h"
#include "components/favicon_base/favicon_util.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_service.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;

namespace {

void OnFaviconBitmapResultAvailable(
    const JavaRef<jobject>& j_favicon_image_callback,
    const favicon_base::FaviconRawBitmapResult& result) {
  JNIEnv* env = AttachCurrentThread();

  // Convert favicon_image_result to java objects.
  ScopedJavaLocalRef<jstring> j_icon_url =
      ConvertUTF8ToJavaString(env, result.icon_url.spec());
  ScopedJavaLocalRef<jobject> j_favicon_bitmap;
  if (result.is_valid()) {
    SkBitmap favicon_bitmap;
    gfx::PNGCodec::Decode(result.bitmap_data->front(),
                          result.bitmap_data->size(),
                          &favicon_bitmap);
    if (!favicon_bitmap.isNull())
      j_favicon_bitmap = gfx::ConvertToJavaBitmap(&favicon_bitmap);
  }

  // Call java side OnFaviconBitmapResultAvailable method.
  Java_FaviconImageCallback_onFaviconAvailable(env, j_favicon_image_callback,
                                               j_favicon_bitmap, j_icon_url);
}

void OnEnsureIconIsAvailableFinished(
    const ScopedJavaGlobalRef<jobject>& j_availability_callback,
    bool newly_available) {
  JNIEnv* env = AttachCurrentThread();
  Java_IconAvailabilityCallback_onIconAvailabilityChecked(
      env, j_availability_callback, newly_available);
}

}  // namespace

static jlong JNI_FaviconHelper_Init(JNIEnv* env) {
  return reinterpret_cast<intptr_t>(new FaviconHelper());
}

FaviconHelper::FaviconHelper() {
  cancelable_task_tracker_.reset(new base::CancelableTaskTracker());
}

void FaviconHelper::Destroy(JNIEnv* env) {
  delete this;
}

jboolean FaviconHelper::GetLocalFaviconImageForURL(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile,
    const JavaParamRef<jstring>& j_page_url,
    jint j_desired_size_in_pixel,
    const JavaParamRef<jobject>& j_favicon_image_callback) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  DCHECK(profile);
  if (!profile)
    return false;

  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  DCHECK(favicon_service);
  if (!favicon_service)
    return false;

  favicon_base::FaviconRawBitmapCallback callback_runner =
      base::BindOnce(&OnFaviconBitmapResultAvailable,
                     ScopedJavaGlobalRef<jobject>(j_favicon_image_callback));

  // |j_page_url| is an origin, and it may not have had a favicon associated
  // with it. A trickier case is when |j_page_url| only has domain-scoped
  // cookies, but visitors are redirected to HTTPS on visiting. Then
  // |j_page_url| defaults to a HTTP scheme, but the favicon will be associated
  // with the HTTPS URL and hence won't be found if we include the scheme in the
  // lookup. Set |fallback_to_host|=true so the favicon database will fall back
  // to matching only the hostname to have the best chance of finding a favicon.
  const bool fallback_to_host = true;
  favicon_service->GetRawFaviconForPageURL(
      GURL(ConvertJavaStringToUTF16(env, j_page_url)),
      {favicon_base::IconType::kFavicon, favicon_base::IconType::kTouchIcon,
       favicon_base::IconType::kTouchPrecomposedIcon,
       favicon_base::IconType::kWebManifestIcon},
      static_cast<int>(j_desired_size_in_pixel), fallback_to_host,
      std::move(callback_runner), cancelable_task_tracker_.get());

  return true;
}

jboolean FaviconHelper::GetForeignFaviconImageForURL(
    JNIEnv* env,
    const JavaParamRef<jobject>& jprofile,
    const JavaParamRef<jstring>& j_page_url,
    jint j_desired_size_in_pixel,
    const base::android::JavaParamRef<jobject>& j_favicon_image_callback) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  if (!profile)
    return false;

  GURL page_url(ConvertJavaStringToUTF8(env, j_page_url));

  sync_sessions::SessionSyncService* session_sync_service =
      SessionSyncServiceFactory::GetInstance()->GetForProfile(profile);
  DCHECK(session_sync_service);
  sync_sessions::OpenTabsUIDelegate* open_tabs =
      session_sync_service->GetOpenTabsUIDelegate();
  favicon::HistoryUiFaviconRequestHandler* history_ui_favicon_request_handler =
      HistoryUiFaviconRequestHandlerFactory::GetForBrowserContext(profile);
  // Can be null in tests.
  if (!history_ui_favicon_request_handler)
    return false;
  history_ui_favicon_request_handler->GetRawFaviconForPageURL(
      page_url, static_cast<int>(j_desired_size_in_pixel),
      base::BindOnce(&OnFaviconBitmapResultAvailable,
                     ScopedJavaGlobalRef<jobject>(j_favicon_image_callback)),
      favicon::FaviconRequestPlatform::kMobile,
      favicon::HistoryUiFaviconRequestOrigin::kRecentTabs,
      /*icon_url_for_uma=*/
      open_tabs ? open_tabs->GetIconUrlForPageUrl(page_url) : GURL(),
      cancelable_task_tracker_.get());
  return true;
}

void FaviconHelper::EnsureIconIsAvailable(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile,
    const JavaParamRef<jobject>& j_web_contents,
    const JavaParamRef<jstring>& j_page_url,
    const JavaParamRef<jstring>& j_icon_url,
    jboolean j_is_large_icon,
    const JavaParamRef<jobject>& j_availability_callback) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  DCHECK(profile);
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  DCHECK(web_contents);
  GURL page_url(ConvertJavaStringToUTF8(env, j_page_url));
  GURL icon_url(ConvertJavaStringToUTF8(env, j_icon_url));
  favicon_base::IconType icon_type = j_is_large_icon
                                         ? favicon_base::IconType::kTouchIcon
                                         : favicon_base::IconType::kFavicon;

  // TODO(treib): Optimize this by creating a FaviconService::HasFavicon method
  // so that we don't have to actually get the image.
  ScopedJavaGlobalRef<jobject> j_scoped_callback(env, j_availability_callback);
  favicon_base::FaviconImageCallback callback_runner = base::BindOnce(
      &FaviconHelper::OnFaviconImageResultAvailable, j_scoped_callback, profile,
      web_contents, page_url, icon_url, icon_type);
  favicon::FaviconService* service = FaviconServiceFactory::GetForProfile(
      profile, ServiceAccessType::IMPLICIT_ACCESS);
  favicon::GetFaviconImageForPageURL(service, page_url, icon_type,
                                     std::move(callback_runner),
                                     cancelable_task_tracker_.get());
}

void FaviconHelper::TouchOnDemandFavicon(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile,
    const JavaParamRef<jstring>& j_icon_url) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  DCHECK(profile);
  GURL icon_url(ConvertJavaStringToUTF8(env, j_icon_url));

  favicon::FaviconService* service = FaviconServiceFactory::GetForProfile(
      profile, ServiceAccessType::IMPLICIT_ACCESS);
  service->TouchOnDemandFavicon(icon_url);
}

FaviconHelper::~FaviconHelper() {}

// Return the index of |sizes| whose area is largest but not exceeds int type
// range. If all |sizes|'s area exceed int type range, return the first one.
size_t FaviconHelper::GetLargestSizeIndex(const std::vector<gfx::Size>& sizes) {
  DCHECK(!sizes.empty());
  size_t ret = 0;
  // Find the first element whose area doesn't exceed max value, then use it
  // to compare with rest elements to find largest size index.
  for (size_t i = 0; i < sizes.size(); ++i) {
    base::CheckedNumeric<int> checked_area = sizes[i].GetCheckedArea();
    if (checked_area.IsValid()) {
      ret = i;
      int largest_area = checked_area.ValueOrDie();
      for (i = ret + 1; i < sizes.size(); ++i) {
        int area = sizes[i].GetCheckedArea().ValueOrDefault(-1);
        if (largest_area < area) {
          ret = i;
          largest_area = area;
        }
      }
    }
  }
  return ret;
}

void FaviconHelper::OnFaviconDownloaded(
    const ScopedJavaGlobalRef<jobject>& j_availability_callback,
    Profile* profile,
    const GURL& page_url,
    favicon_base::IconType icon_type,
    int download_request_id,
    int http_status_code,
    const GURL& image_url,
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<gfx::Size>& original_sizes) {
  if (bitmaps.empty()) {
    OnEnsureIconIsAvailableFinished(j_availability_callback,
                                    /*newly_available=*/false);
    return;
  }

  // Only keep the largest icon available.
  gfx::Image image = gfx::Image(gfx::ImageSkia(
      gfx::ImageSkiaRep(bitmaps[GetLargestSizeIndex(original_sizes)], 0)));
  favicon_base::SetFaviconColorSpace(&image);
  favicon::FaviconService* service = FaviconServiceFactory::GetForProfile(
      profile, ServiceAccessType::IMPLICIT_ACCESS);

  service->SetOnDemandFavicons(
      page_url, image_url, icon_type, image,
      base::Bind(&OnEnsureIconIsAvailableFinished, j_availability_callback));
}

void FaviconHelper::OnFaviconImageResultAvailable(
    const ScopedJavaGlobalRef<jobject>& j_availability_callback,
    Profile* profile,
    content::WebContents* web_contents,
    const GURL& page_url,
    const GURL& icon_url,
    favicon_base::IconType icon_type,
    const favicon_base::FaviconImageResult& result) {
  // If there already is a favicon, return immediately.
  // Can |web_contents| be null here? crbug.com/688249
  if (!result.image.IsEmpty() || !web_contents) {
    // Either the image already exists in the FaviconService, or it doesn't and
    // we can't download it. Either way, it's not *newly* available.
    OnEnsureIconIsAvailableFinished(j_availability_callback,
                                    /*newly_available=*/false);
    return;
  }

  web_contents->DownloadImage(
      icon_url, true, 0, 0, false,
      base::BindOnce(&FaviconHelper::OnFaviconDownloaded,
                     j_availability_callback, profile, page_url, icon_type));
}
