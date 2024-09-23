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
using JobFinishedCallback = base::OnceCallback<void(void)>;

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
      std::vector<GURL> urls,
      int desire_size_in_pixel,
      JobFinishedCallback job_finished_callback,
      favicon_base::FaviconResultsCallback result_callback);

  Job(const Job&) = delete;
  Job& operator=(const Job&) = delete;

  void Start();

 private:
  void OnFaviconAvailable(int favicon_index,
                          const favicon_base::FaviconRawBitmapResult& result);
  raw_ptr<FaviconHelper> favicon_helper_;
  raw_ptr<favicon::FaviconService> favicon_service_;
  std::vector<GURL> urls_;
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
                        std::vector<GURL> urls,
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
  if (urls_size <= 1 || urls_size > 4) {
    return;
  }

  for (size_t i = 0; i < urls_size; i++) {
    favicon_base::FaviconRawBitmapCallback callback =
        base::BindOnce(&FaviconHelper::Job::OnFaviconAvailable,
                       weak_ptr_factory_.GetWeakPtr(), i);

    favicon_helper_->GetLocalFaviconImageForURLInternal(
        favicon_service_, urls_.at(i), desire_size_in_pixel_,
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
  cancelable_task_tracker_ = std::make_unique<base::CancelableTaskTracker>();
}

void FaviconHelper::Destroy(JNIEnv* env) {
  delete this;
}

jboolean FaviconHelper::GetComposedFaviconImage(
    JNIEnv* env,
    Profile* profile,
    std::vector<GURL>& gurls,
    jint j_desired_size_in_pixel,
    const base::android::JavaParamRef<jobject>&
        j_composed_favicon_image_callback) {
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

  int desired_size_in_pixel = static_cast<int>(j_desired_size_in_pixel);

  favicon_base::FaviconResultsCallback callback_runner = base::BindOnce(
      &FaviconHelper::OnComposedFaviconBitmapResultsAvailable,
      weak_ptr_factory_.GetWeakPtr(),
      ScopedJavaGlobalRef<jobject>(j_composed_favicon_image_callback),
      desired_size_in_pixel);

  GetComposedFaviconImageInternal(favicon_service, gurls,
                                  static_cast<int>(j_desired_size_in_pixel),
                                  std::move(callback_runner));

  return true;
}

void FaviconHelper::GetComposedFaviconImageInternal(
    favicon::FaviconService* favicon_service,
    std::vector<GURL> urls,
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
      base::BindOnce(&FaviconHelper::OnFaviconBitmapResultAvailable,
                     weak_ptr_factory_.GetWeakPtr(),
                     ScopedJavaGlobalRef<jobject>(j_favicon_image_callback)),
      favicon::HistoryUiFaviconRequestOrigin::kRecentTabs);
  return true;
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

void FaviconHelper::OnFaviconBitmapResultAvailable(
    const JavaRef<jobject>& j_favicon_image_callback,
    const favicon_base::FaviconRawBitmapResult& result) {
  JNIEnv* env = AttachCurrentThread();

  // Convert favicon_image_result to java objects.
  ScopedJavaLocalRef<jobject> j_favicon_bitmap;
  if (result.is_valid()) {
    SkBitmap favicon_bitmap;
    gfx::PNGCodec::Decode(result.bitmap_data->front(),
                          result.bitmap_data->size(), &favicon_bitmap);
    if (!favicon_bitmap.isNull()) {
      j_favicon_bitmap = gfx::ConvertToJavaBitmap(favicon_bitmap);
    }
  }

  // Call java side OnFaviconBitmapResultAvailable method.
  Java_FaviconImageCallback_onFaviconAvailable(
      env, j_favicon_image_callback, j_favicon_bitmap, result.icon_url);
}

void FaviconHelper::OnComposedFaviconBitmapResultsAvailable(
    const JavaRef<jobject>& j_favicon_image_callback,
    const int desired_size_in_pixel,
    const std::vector<favicon_base::FaviconRawBitmapResult>& results) {
  JNIEnv* env = AttachCurrentThread();
  std::vector<SkBitmap> result_bitmaps;
  std::vector<GURL> icon_url_vector;
  for (size_t i = 0; i < results.size(); i++) {
    favicon_base::FaviconRawBitmapResult result = results[i];
    if (!result.is_valid()) {
      continue;
    }
    SkBitmap favicon_bitmap;
    icon_url_vector.push_back(result.icon_url);
    gfx::PNGCodec::Decode(result.bitmap_data->front(),
                          result.bitmap_data->size(), &favicon_bitmap);
    result_bitmaps.push_back(std::move(favicon_bitmap));
  }
  ScopedJavaLocalRef<jobject> j_favicon_bitmap;
  if (!result_bitmaps.empty()) {
    std::unique_ptr<SkBitmap> composed_bitmap =
        compose_bitmaps_helper::ComposeBitmaps(std::move(result_bitmaps),
                                               desired_size_in_pixel);
    if (composed_bitmap && !composed_bitmap->isNull()) {
      j_favicon_bitmap = gfx::ConvertToJavaBitmap(*composed_bitmap);
    }
  }

  // Call java side OnComposedFaviconBitmapResultsAvailable method.
  Java_ComposedFaviconImageCallback_onComposedFaviconAvailable(
      env, j_favicon_image_callback, j_favicon_bitmap, icon_url_vector);
}
