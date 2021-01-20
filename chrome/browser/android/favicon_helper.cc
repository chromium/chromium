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
#include "chrome/browser/android/compose_bitmaps_helper.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/favicon/history_ui_favicon_request_handler_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/ui/android/favicon/jni_headers/FaviconHelper_jni.h"
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

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using JobFinishedCallback = base::OnceCallback<void(void)>;

namespace {

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

// This is used by the FaviconHelper::GetComposedFaviconImageInternal, and it is
// used to manage multiple FaviconService::GetRawFaviconForPageURL calls. The
// number of calls is the size of the urls_. The Job is destroyed after the
// number of calls have been reached, and the result_callback_ is finished.
class FaviconHelper::Job {
 public:
  Job(FaviconHelper* favicon_helper,
      favicon::FaviconService* favicon_service,
      std::vector<std::string> urls,
      int desire_size_in_pixel,
      JobFinishedCallback job_finished_callback,
      favicon_base::FaviconResultsCallback result_callback);

  Job(const Job&) = delete;
  Job& operator=(const Job&) = delete;

  void Start();

 private:
  void OnFaviconAvailable(int favicon_index,
                          const favicon_base::FaviconRawBitmapResult& result);
  FaviconHelper* favicon_helper_;
  favicon::FaviconService* favicon_service_;
  std::vector<std::string> urls_;
  int desire_size_in_pixel_;
  JobFinishedCallback job_finished_callback_;
  favicon_base::FaviconResultsCallback result_callback_;
  int favicon_expected_count_;
  std::vector<favicon_base::FaviconRawBitmapResult> favicon_raw_bitmap_results_;
  int favicon_result_count_;

  base::WeakPtrFactory<Job> weak_ptr_factory_{this};
};

FaviconHelper::Job::Job(FaviconHelper* favicon_helper,
                        favicon::FaviconService* favicon_service,
                        std::vector<std::string> urls,
                        int desire_size_in_pixel,
                        JobFinishedCallback job_finished_callback,
                        favicon_base::FaviconResultsCallback result_callback)
    : favicon_helper_(favicon_helper),
      favicon_service_(favicon_service),
      urls_(urls),
      desire_size_in_pixel_(desire_size_in_pixel),
      job_finished_callback_(std::move(job_finished_callback)),
      result_callback_(std::move(result_callback)),
      favicon_raw_bitmap_results_(4),
      favicon_result_count_(0) {
  favicon_expected_count_ = urls_.size();
}

void FaviconHelper::Job::Start() {
  size_t urls_size = urls_.size();
  DCHECK(urls_size > 1 && urls_size <= 4);
  if (urls_size <= 1 || urls_size > 4)
    return;

  for (size_t i = 0; i < urls_size; i++) {
    favicon_base::FaviconRawBitmapCallback callback =
        base::BindOnce(&FaviconHelper::Job::OnFaviconAvailable,
                       weak_ptr_factory_.GetWeakPtr(), i);

    favicon_helper_->GetLocalFaviconImageForURLInternal(
        favicon_service_, GURL(urls_.at(i)), desire_size_in_pixel_,
        std::move(callback));
  }
}

void FaviconHelper::Job::OnFaviconAvailable(
    int favicon_index,
    const favicon_base::FaviconRawBitmapResult& result) {
  DCHECK(favicon_index >= 0 && favicon_index < 4);

  if (result.is_valid()) {
    favicon_raw_bitmap_results_.at(favicon_index) = result;
    favicon_result_count_++;
  } else {
    favicon_expected_count_--;
  }

  if (favicon_result_count_ == favicon_expected_count_) {
    size_t i = 0;
    while (i < favicon_raw_bitmap_results_.size()) {
      if (!favicon_raw_bitmap_results_[i].is_valid()) {
        favicon_raw_bitmap_results_.erase(favicon_raw_bitmap_results_.begin() +
                                          i);
        continue;
      }
      i++;
    }
    std::move(result_callback_).Run(favicon_raw_bitmap_results_);
    std::move(job_finished_callback_).Run();
  }
}

FaviconHelper::FaviconHelper() : last_used_job_id_(0) {
  cancelable_task_tracker_.reset(new base::CancelableTaskTracker());
}

void FaviconHelper::Destroy(JNIEnv* env) {
  delete this;
}

jboolean FaviconHelper::GetComposedFaviconImage(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_profile,
    const base::android::JavaParamRef<jobjectArray>& j_urls,
    jint j_desired_size_in_pixel,
    const base::android::JavaParamRef<jobject>& j_favicon_image_callback) {
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

  int desired_size_in_pixel = static_cast<int>(j_desired_size_in_pixel);

  favicon_base::FaviconResultsCallback callback_runner =
      base::BindOnce(&FaviconHelper::OnFaviconBitmapResultsAvailable,
                     weak_ptr_factory_.GetWeakPtr(),
                     ScopedJavaGlobalRef<jobject>(j_favicon_image_callback),
                     desired_size_in_pixel);

  std::vector<std::string> urls;
  base::android::AppendJavaStringArrayToStringVector(env, j_urls, &urls);

  GetComposedFaviconImageInternal(favicon_service, urls,
                                  static_cast<int>(j_desired_size_in_pixel),
                                  std::move(callback_runner));

  return true;
}

void FaviconHelper::GetComposedFaviconImageInternal(
    favicon::FaviconService* favicon_service,
    std::vector<std::string> urls,
    int desired_size_in_pixel,
    favicon_base::FaviconResultsCallback callback_runner) {
  DCHECK(favicon_service);

  JobFinishedCallback job_finished_callback =
      base::BindOnce(&FaviconHelper::OnJobFinished,
                     weak_ptr_factory_.GetWeakPtr(), ++last_used_job_id_);

  auto job = std::make_unique<Job>(
      this, favicon_service, urls, desired_size_in_pixel,
      std::move(job_finished_callback), std::move(callback_runner));

  id_to_job_[last_used_job_id_] = std::move(job);
  id_to_job_[last_used_job_id_]->Start();
}

void ::FaviconHelper::OnJobFinished(int job_id) {
  DCHECK(id_to_job_.count(job_id));

  id_to_job_.erase(job_id);
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
      base::BindOnce(&FaviconHelper::OnFaviconBitmapResultAvailable,
                     weak_ptr_factory_.GetWeakPtr(),
                     ScopedJavaGlobalRef<jobject>(j_favicon_image_callback));

  GetLocalFaviconImageForURLInternal(
      favicon_service, GURL(ConvertJavaStringToUTF16(env, j_page_url)),
      static_cast<int>(j_desired_size_in_pixel), std::move(callback_runner));

  return true;
}

void FaviconHelper::GetLocalFaviconImageForURLInternal(
    favicon::FaviconService* favicon_service,
    GURL url,
    int desired_size_in_pixel,
    favicon_base::FaviconRawBitmapCallback callback_runner) {
  DCHECK(favicon_service);
  if (!favicon_service)
    return;

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
    const JavaParamRef<jobject>& jprofile,
    const JavaParamRef<jstring>& j_page_url,
    jint j_desired_size_in_pixel,
    const base::android::JavaParamRef<jobject>& j_favicon_image_callback) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  if (!profile)
    return false;

  GURL page_url(ConvertJavaStringToUTF8(env, j_page_url));

  favicon::HistoryUiFaviconRequestHandler* history_ui_favicon_request_handler =
      HistoryUiFaviconRequestHandlerFactory::GetForBrowserContext(profile);
  // Can be null in tests.
  if (!history_ui_favicon_request_handler)
    return false;
  history_ui_favicon_request_handler->GetRawFaviconForPageURL(
      page_url, static_cast<int>(j_desired_size_in_pixel),
      base::BindOnce(&FaviconHelper::OnFaviconBitmapResultAvailable,
                     weak_ptr_factory_.GetWeakPtr(),
                     ScopedJavaGlobalRef<jobject>(j_favicon_image_callback)),
      favicon::HistoryUiFaviconRequestOrigin::kRecentTabs);
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

// static
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

  service->SetOnDemandFavicons(page_url, image_url, icon_type, image,
                               base::BindOnce(&OnEnsureIconIsAvailableFinished,
                                              j_availability_callback));
}

// static
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

void FaviconHelper::OnFaviconBitmapResultAvailable(
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
                          result.bitmap_data->size(), &favicon_bitmap);
    if (!favicon_bitmap.isNull())
      j_favicon_bitmap = gfx::ConvertToJavaBitmap(favicon_bitmap);
  }

  // Call java side OnFaviconBitmapResultAvailable method.
  Java_FaviconImageCallback_onFaviconAvailable(env, j_favicon_image_callback,
                                               j_favicon_bitmap, j_icon_url);
}

void FaviconHelper::OnFaviconBitmapResultsAvailable(
    const JavaRef<jobject>& j_favicon_image_callback,
    const int desired_size_in_pixel,
    const std::vector<favicon_base::FaviconRawBitmapResult>& results) {
  std::vector<SkBitmap> result_bitmaps;

  for (size_t i = 0; i < results.size(); i++) {
    favicon_base::FaviconRawBitmapResult result = results[i];
    if (!result.is_valid())
      continue;
    SkBitmap favicon_bitmap;
    gfx::PNGCodec::Decode(result.bitmap_data->front(),
                          result.bitmap_data->size(), &favicon_bitmap);
    result_bitmaps.push_back(std::move(favicon_bitmap));
  }

  ScopedJavaLocalRef<jobject> j_favicon_bitmap;
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_icon_url;

  if (!result_bitmaps.empty()) {
    std::unique_ptr<SkBitmap> composed_bitmap =
        compose_bitmaps_helper::ComposeBitmaps(std::move(result_bitmaps),
                                               desired_size_in_pixel);
    if (composed_bitmap && !composed_bitmap->isNull()) {
      j_favicon_bitmap = gfx::ConvertToJavaBitmap(*composed_bitmap);
    }
  }

  // Call java side OnFaviconBitmapResultAvailable method.
  Java_FaviconImageCallback_onFaviconAvailable(env, j_favicon_image_callback,
                                               j_favicon_bitmap, j_icon_url);
}
