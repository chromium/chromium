// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/android/download_utils.h"

#include <utility>

#include "base/android/jni_string.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/download/android/jni_headers/MimeUtils_jni.h"
#include "chrome/browser/download/offline_item_utils.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "components/download/public/common/download_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_manager_delegate.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

// Must come after other headers because it uses
// offline_items_collection::FailState.
#include "chrome/android/chrome_jni_headers/DownloadUtils_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace {
// If received bytes is more than the size limit and resumption will restart
// from the beginning, throttle it.
int kDefaultAutoResumptionSizeLimit = 10 * 1024 * 1024;  // 10 MB
const char kAutoResumptionSizeLimitParamName[] = "AutoResumptionSizeLimit";
}  // namespace

static jint JNI_DownloadUtils_GetResumeMode(
    JNIEnv* env,
    std::string& url,
    offline_items_collection::FailState failState) {
  auto reason =
      OfflineItemUtils::ConvertFailStateToDownloadInterruptReason(failState);
  return static_cast<jint>(download::GetDownloadResumeMode(
      GURL(std::move(url)), reason, false /* restart_required */,
      true /* user_action_required */));
}

static jboolean JNI_DownloadUtils_IsDownloadRestrictedByPolicy(
    JNIEnv* env,
    Profile* profile) {
  content::DownloadManager* manager = profile->GetDownloadManager();
  if (manager) {
    return manager->GetDelegate()->IsDownloadRestrictedByPolicy();
  }
  return false;
}

// static
base::FilePath DownloadUtils::GetUriStringForPath(
    const base::FilePath& file_path) {
  JNIEnv* env = base::android::AttachCurrentThread();
  auto uri_string =
      Java_DownloadUtils_getUriStringForPath(env, file_path.AsUTF8Unsafe());
  return base::FilePath(uri_string);
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
  std::string original_url = item->GetOriginalUrl().SchemeIs(url::kDataScheme)
                                 ? std::string()
                                 : item->GetOriginalUrl().spec();
  base::android::ScopedJavaLocalRef<jobject> otr_profile_id;
  if (browser_context && browser_context->IsOffTheRecord()) {
    Profile* profile = Profile::FromBrowserContext(browser_context);
    otr_profile_id = profile->GetOTRProfileID().ConvertToJavaOTRProfileID(env);
  }

  Java_DownloadUtils_openDownload(
      env, item->GetTargetFilePath().value(), item->GetMimeType(),
      item->GetGuid(), otr_profile_id, original_url,
      item->GetReferrerUrl().spec(), static_cast<jint>(open_source));
}

// static
std::string DownloadUtils::RemapGenericMimeType(const std::string& mime_type,
                                                const GURL& url,
                                                const std::string& file_name) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_MimeUtils_remapGenericMimeType(env, mime_type, url.spec(),
                                             file_name);
}

// static
bool DownloadUtils::ShouldAutoOpenDownload(download::DownloadItem* item) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_MimeUtils_canAutoOpenMimeType(env, item->GetMimeType()) &&
         IsDownloadUserInitiated(item);
}

// static
bool DownloadUtils::IsOmaDownloadDescription(const std::string& mime_type) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_MimeUtils_isOMADownloadDescription(env, mime_type);
}

// static
void DownloadUtils::ShowDownloadManager(bool show_prefetched_content,
                                        DownloadOpenSource open_source) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_DownloadUtils_showDownloadManager(
      env, nullptr, nullptr, nullptr, static_cast<jint>(open_source),
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
