// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/android/download_utils.h"

#include <utility>

#include "base/android/jni_string.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/android/chrome_jni_headers/DownloadUtils_jni.h"
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/browser/download/android/jni_headers/MimeUtils_jni.h"
#include "chrome/browser/download/offline_item_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "components/download/public/common/download_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace {
// If received bytes is more than the size limit and resumption will restart
// from the beginning, throttle it.
int kDefaultAutoResumptionSizeLimit = 10 * 1024 * 1024;  // 10 MB
const char kAutoResumptionSizeLimitParamName[] = "AutoResumptionSizeLimit";
}  // namespace

static ScopedJavaLocalRef<jstring> JNI_DownloadUtils_GetFailStateMessage(
    JNIEnv* env,
    jint fail_state) {
  base::string16 message = OfflineItemUtils::GetFailStateMessage(
      static_cast<offline_items_collection::FailState>(fail_state));
  l10n_util::GetStringFUTF16(IDS_DOWNLOAD_STATUS_INTERRUPTED, message);
  return ConvertUTF16ToJavaString(env, message);
}

static jint JNI_DownloadUtils_GetResumeMode(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& jurl,
    jint failState) {
  std::string url = ConvertJavaStringToUTF8(env, jurl);
  auto reason = OfflineItemUtils::ConvertFailStateToDownloadInterruptReason(
      static_cast<offline_items_collection::FailState>(failState));
  return static_cast<jint>(download::GetDownloadResumeMode(
      GURL(std::move(url)), reason, false /* restart_required */,
      true /* user_action_required */));
}

// static
base::FilePath DownloadUtils::GetUriStringForPath(
    const base::FilePath& file_path) {
  JNIEnv* env = base::android::AttachCurrentThread();
  auto uri_jstring = Java_DownloadUtils_getUriStringForPath(
      env, ConvertUTF8ToJavaString(env, file_path.AsUTF8Unsafe()));
  return base::FilePath(ConvertJavaStringToUTF8(env, uri_jstring));
}

// static
int DownloadUtils::GetAutoResumptionSizeLimit() {
  std::string value = base::GetFieldTrialParamValueByFeature(
      chrome::android::kDownloadAutoResumptionThrottling,
      kAutoResumptionSizeLimitParamName);
  int size_limit;
  return base::StringToInt(value, &size_limit)
             ? size_limit
             : kDefaultAutoResumptionSizeLimit;
}

// static
void DownloadUtils::OpenDownload(download::DownloadItem* item,
                                 DownloadOpenSource open_source) {
  JNIEnv* env = base::android::AttachCurrentThread();
  content::BrowserContext* browser_context =
      content::DownloadItemUtils::GetBrowserContext(item);
  bool is_off_the_record =
      browser_context ? browser_context->IsOffTheRecord() : false;
  std::string original_url = item->GetOriginalUrl().SchemeIs(url::kDataScheme)
                                 ? std::string()
                                 : item->GetOriginalUrl().spec();

  Java_DownloadUtils_openDownload(
      env, ConvertUTF8ToJavaString(env, item->GetTargetFilePath().value()),
      ConvertUTF8ToJavaString(env, item->GetMimeType()),
      ConvertUTF8ToJavaString(env, item->GetGuid()), is_off_the_record,
      ConvertUTF8ToJavaString(env, original_url),
      ConvertUTF8ToJavaString(env, item->GetReferrerUrl().spec()),
      static_cast<jint>(open_source));
}

// static
std::string DownloadUtils::RemapGenericMimeType(const std::string& mime_type,
                                                const GURL& url,
                                                const std::string& file_name) {
  JNIEnv* env = base::android::AttachCurrentThread();
  auto j_remapped_mime_type = Java_MimeUtils_remapGenericMimeType(
      env, ConvertUTF8ToJavaString(env, mime_type),
      ConvertUTF8ToJavaString(env, url.spec()),
      ConvertUTF8ToJavaString(env, file_name));
  return ConvertJavaStringToUTF8(env, j_remapped_mime_type);
}

// static
bool DownloadUtils::ShouldAutoOpenDownload(download::DownloadItem* item) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_MimeUtils_canAutoOpenMimeType(
             env, ConvertUTF8ToJavaString(env, item->GetMimeType())) &&
         IsDownloadUserInitiated(item);
}

// static
bool DownloadUtils::IsOmaDownloadDescription(const std::string& mime_type) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_MimeUtils_isOMADownloadDescription(
      env, ConvertUTF8ToJavaString(env, mime_type));
}

// static
void DownloadUtils::ShowDownloadManager(bool show_prefetched_content,
                                        DownloadOpenSource open_source) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_DownloadUtils_showDownloadManager(
      env, nullptr, nullptr, static_cast<jint>(open_source),
      static_cast<jboolean>(show_prefetched_content));
}

bool DownloadUtils::IsDownloadUserInitiated(download::DownloadItem* download) {
  ui::PageTransition page_transition = download->GetTransitionType();
  return download->HasUserGesture() ||
         (page_transition & ui::PAGE_TRANSITION_FROM_ADDRESS_BAR) ||
         PageTransitionCoreTypeIs(page_transition, ui::PAGE_TRANSITION_TYPED) ||
         PageTransitionCoreTypeIs(page_transition,
                                  ui::PAGE_TRANSITION_AUTO_BOOKMARK) ||
         PageTransitionCoreTypeIs(page_transition,
                                  ui::PAGE_TRANSITION_GENERATED) ||
         PageTransitionCoreTypeIs(page_transition,
                                  ui::PAGE_TRANSITION_RELOAD) ||
         PageTransitionCoreTypeIs(page_transition, ui::PAGE_TRANSITION_KEYWORD);
}
