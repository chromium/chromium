// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/android/download_controller.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/json/values_util.h"
#include "base/lazy_instance.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/profile_key_startup_accessor.h"
#include "chrome/browser/android/profile_key_util.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/download/android/dangerous_download_infobar_delegate.h"
#include "chrome/browser/download/android/download_manager_service.h"
#include "chrome/browser/download/android/download_utils.h"
#include "chrome/browser/download/android/new_navigation_observer.h"
#include "chrome/browser/download/download_offline_content_provider.h"
#include "chrome/browser/download/download_offline_content_provider_factory.h"
#include "chrome/browser/download/download_stats.h"
#include "chrome/browser/download/insecure_download_blocking.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/offline_pages/android/offline_page_bridge.h"
#include "chrome/browser/permissions/permission_update_message_controller_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "components/download/content/public/context_menu_download.h"
#include "components/download/public/common/android/auto_resumption_handler.h"
#include "components/download/public/common/download_item.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/pdf/common/constants.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/public/browser/download_request_utils.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/content_features.h"
#include "net/base/filename_util.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/base/device_form_factor.h"
#include "ui/base/page_transition_types.h"
#include "url/android/gurl_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/DownloadController_jni.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using content::BrowserContext;
using content::BrowserThread;
using content::ContextMenuParams;
using content::DownloadManager;
using content::WebContents;
using download::DownloadItem;

namespace {
// Guards download_controller_
base::LazyInstance<base::Lock>::DestructorAtExit g_download_controller_lock_;

void CreateContextMenuDownloadInternal(
    const content::WebContents::Getter& wc_getter,
    const content::ContextMenuParams& params,
    bool is_link,
    bool granted) {
  content::WebContents* web_contents = wc_getter.Run();
  if (!granted)
    return;

  if (!web_contents) {
    return;
  }

  RecordDownloadSource(DOWNLOAD_INITIATED_BY_CONTEXT_MENU);
  auto origin = offline_pages::android::OfflinePageBridge::GetEncodedOriginApp(
      web_contents);
  download::CreateContextMenuDownload(web_contents, params, origin, is_link);
}

// Helper class for retrieving a DownloadManager.
class DownloadManagerGetter : public DownloadManager::Observer {
 public:
  explicit DownloadManagerGetter(DownloadManager* manager) : manager_(manager) {
    manager_->AddObserver(this);
  }

  DownloadManagerGetter(const DownloadManagerGetter&) = delete;
  DownloadManagerGetter& operator=(const DownloadManagerGetter&) = delete;

  ~DownloadManagerGetter() override {
    if (manager_)
      manager_->RemoveObserver(this);
  }

  void ManagerGoingDown(DownloadManager* manager) override {
    manager_ = nullptr;
  }

  DownloadManager* manager() { return manager_; }

 private:
  raw_ptr<DownloadManager> manager_;
};

void RemoveDownloadItem(std::unique_ptr<DownloadManagerGetter> getter,
                        const std::string& guid) {
  if (!getter->manager())
    return;
  DownloadItem* item = getter->manager()->GetDownloadByGuid(guid);
  if (item)
    item->Remove();
}

void ScheduleRemoveDownloadItem(download::DownloadItem* download) {
  auto download_manager_getter = std::make_unique<DownloadManagerGetter>(
      content::DownloadItemUtils::GetBrowserContext(download)
          ->GetDownloadManager());
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&RemoveDownloadItem, std::move(download_manager_getter),
                     download->GetGuid()));
}

void OnRequestFileAccessResult(
    const content::WebContents::Getter& web_contents_getter,
    DownloadControllerBase::AcquireFileAccessPermissionCallback cb,
    bool granted,
    const std::string& permission_to_update) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!granted && !permission_to_update.empty() && web_contents_getter.Run()) {
    WebContents* web_contents = web_contents_getter.Run();
    std::vector<std::string> permissions;
    permissions.push_back(permission_to_update);

    PermissionUpdateMessageController::CreateForWebContents(web_contents);
    PermissionUpdateMessageController::FromWebContents(web_contents)
        ->ShowMessage(permissions, IDR_ANDORID_MESSAGE_PERMISSION_STORAGE,
                      IDS_MESSAGE_MISSING_STORAGE_ACCESS_PERMISSION_TITLE,
                      IDS_MESSAGE_STORAGE_ACCESS_PERMISSION_TEXT,
                      std::move(cb));
    return;
  }

  std::move(cb).Run(granted);
}

void OnStoragePermissionDecided(
    DownloadControllerBase::AcquireFileAccessPermissionCallback cb,
    bool granted) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::move(cb).Run(granted);
}

bool ShouldOpenPdfInline(DownloadItem* item) {
  BrowserContext* context = content::DownloadItemUtils::GetBrowserContext(item);
  return context && context->GetDownloadManagerDelegate() &&
         context->GetDownloadManagerDelegate()->ShouldOpenPdfInline() &&
         !item->IsMustDownload() && item->IsTransient();
}

void CleanupAppVerificationTimestamps(download::DownloadItem* item) {
  Profile* profile = Profile::FromBrowserContext(
      content::DownloadItemUtils::GetBrowserContext(item));
  if (!profile || !profile->GetPrefs()) {
    return;
  }
  ScopedListPrefUpdate update(profile->GetPrefs(),
                              prefs::kDownloadAppVerificationPromptTimestamps);
  update->EraseIf([](const base::Value& timestamp) {
    constexpr base::TimeDelta kImpressionWindow = base::Days(90);

    std::optional<base::Time> parsed_timestamp = base::ValueToTime(timestamp);
    if (!parsed_timestamp.has_value()) {
      return true;
    }

    return base::Time::Now() - parsed_timestamp.value() > kImpressionWindow;
  });
}

bool HasSeenTooManyAppVerificationPrompts(download::DownloadItem* item) {
  constexpr size_t kMaxImpressions = 3;
  Profile* profile = Profile::FromBrowserContext(
      content::DownloadItemUtils::GetBrowserContext(item));
  if (!profile || !profile->GetPrefs()) {
    return false;
  }
  return profile->GetPrefs()
             ->GetList(prefs::kDownloadAppVerificationPromptTimestamps)
             .size() >= kMaxImpressions;
}

void LogAppVerificationPromptToPrefs(download::DownloadItem* item) {
  Profile* profile = Profile::FromBrowserContext(
      content::DownloadItemUtils::GetBrowserContext(item));
  if (!profile || !profile->GetPrefs()) {
    return;
  }
  ScopedListPrefUpdate update(profile->GetPrefs(),
                              prefs::kDownloadAppVerificationPromptTimestamps);
  update->Append(base::TimeToValue(base::Time::Now()));
}

}  // namespace

static void JNI_DownloadController_OnAcquirePermissionResult(
    JNIEnv* env,
    jlong callback_id,
    jboolean granted,
    std::string& permission_to_update) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(callback_id);

  if (!DownloadController::GetInstance()
           ->validator()
           ->ValidateAndClearJavaCallback(callback_id)) {
    return;
  }

  // Convert java long long int to c++ pointer, take ownership.
  std::unique_ptr<DownloadController::AcquirePermissionCallback> cb(
      reinterpret_cast<DownloadController::AcquirePermissionCallback*>(
          callback_id));
  std::move(*cb).Run(granted, permission_to_update);
}

static void JNI_DownloadController_CancelDownload(JNIEnv* env,
                                                  Profile* profile,
                                                  std::string& download_guid) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DownloadManager* download_manager = profile->GetDownloadManager();
  if (download_manager) {
    DownloadItem* download = download_manager->GetDownloadByGuid(download_guid);
    if (download) {
      download->Cancel(/*user_cancel=*/false);
    }
  }
}

static void JNI_DownloadController_DownloadUrl(
    JNIEnv* env,
    std::string& url,
    const base::android::JavaParamRef<jobject>& jweb_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  if (!web_contents) {
    return;
  }

  DownloadManager* download_manager =
      web_contents->GetBrowserContext()->GetDownloadManager();
  if (download_manager) {
    std::unique_ptr<download::DownloadUrlParameters> dl_params =
        content::DownloadRequestUtils::CreateDownloadForWebContentsMainFrame(
            web_contents, GURL(url),
            TRAFFIC_ANNOTATION_WITHOUT_PROTO("Download via toolbar menu"));
    dl_params->set_content_initiated(false);
    dl_params->set_download_source(download::DownloadSource::TOOLBAR_MENU);
    download_manager->DownloadUrl(std::move(dl_params));
  }
}

// static
DownloadControllerBase* DownloadControllerBase::Get() {
  base::AutoLock lock(g_download_controller_lock_.Get());
  if (!DownloadControllerBase::download_controller_)
    download_controller_ = DownloadController::GetInstance();
  return DownloadControllerBase::download_controller_;
}

// static
void DownloadControllerBase::SetDownloadControllerBase(
    DownloadControllerBase* download_controller) {
  base::AutoLock lock(g_download_controller_lock_.Get());
  DownloadControllerBase::download_controller_ = download_controller;
}

// static
void DownloadController::CloseTabIfEmpty(content::WebContents* web_contents,
                                         download::DownloadItem* download) {
  if (!web_contents || !web_contents->GetController().IsInitialNavigation())
    return;

  // If the download is dangerous, don't close the tab now. The dangerous
  // infobar needs to be shown.
  if (download && download->IsDangerous() &&
      (download->GetState() != DownloadItem::CANCELLED)) {
    return;
  }

  TabModel* tab_model = TabModelList::GetTabModelForWebContents(web_contents);
  if (!tab_model || tab_model->GetTabCount() == 1)
    return;

  if (!download) {
    web_contents->Close();
    return;
  }

  if (ShouldOpenPdfInline(download) &&
      base::EqualsCaseInsensitiveASCII(download->GetMimeType(),
                                       pdf::kPDFMimeType)) {
    return;
  }

  if (download->IsFromExternalApp()) {
    DownloadManagerService::GetInstance()->OpenDownloadsPage(
        Profile::FromBrowserContext(web_contents->GetBrowserContext()),
        DownloadOpenSource::kExternalApp);
    // For tablet, download home is opened in the current tab, so don't close
    // it.
    if (ui::GetDeviceFormFactor() ==
        ui::DeviceFormFactor::DEVICE_FORM_FACTOR_TABLET) {
      return;
    }
  }
  web_contents->Close();
}

// static
DownloadController* DownloadController::GetInstance() {
  return base::Singleton<DownloadController>::get();
}

DownloadController::DownloadController() = default;

DownloadController::~DownloadController() = default;

void DownloadController::AcquireFileAccessPermission(
    const content::WebContents::Getter& web_contents_getter,
    DownloadControllerBase::AcquireFileAccessPermissionCallback cb) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebContents* web_contents = web_contents_getter.Run();
  ui::ViewAndroid* view_android =
      web_contents ? web_contents->GetNativeView() : nullptr;
  ui::WindowAndroid* window_android =
      view_android ? view_android->GetWindowAndroid() : nullptr;
  ScopedJavaLocalRef<jobject> jwindow_android =
      window_android ? window_android->GetJavaObject()
                     : ScopedJavaLocalRef<jobject>();
  JNIEnv* env = base::android::AttachCurrentThread();

  bool has_file_access_permission =
      Java_DownloadController_hasFileAccess(env, jwindow_android);
  if (has_file_access_permission) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(cb), true));
    return;
  }

  AcquirePermissionCallback callback(base::BindOnce(
      &OnRequestFileAccessResult, web_contents_getter,
      base::BindOnce(&OnStoragePermissionDecided, std::move(cb))));
  // Make copy on the heap so we can pass the pointer through JNI.
  intptr_t callback_id = reinterpret_cast<intptr_t>(
      new AcquirePermissionCallback(std::move(callback)));
  validator_.AddJavaCallback(callback_id);
  Java_DownloadController_requestFileAccess(env, callback_id, jwindow_android);
}

void DownloadController::CreateAndroidDownload(
    const content::WebContents::Getter& wc_getter,
    const DownloadInfo& info) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&DownloadController::StartAndroidDownload,
                                base::Unretained(this), wc_getter, info));
}

void DownloadController::StartAndroidDownload(
    const content::WebContents::Getter& wc_getter,
    const DownloadInfo& info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  AcquireFileAccessPermission(
      wc_getter,
      base::BindOnce(&DownloadController::StartAndroidDownloadInternal,
                     base::Unretained(this), wc_getter, info));
}

void DownloadController::StartAndroidDownloadInternal(
    const content::WebContents::Getter& wc_getter,
    const DownloadInfo& info,
    bool allowed) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!allowed)
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  std::u16string file_name =
      net::GetSuggestedFilename(info.url, info.content_disposition,
                                std::string(),  // referrer_charset
                                std::string(),  // suggested_name
                                info.original_mime_type, default_file_name_);
  ScopedJavaLocalRef<jobject> jurl =
      url::GURLAndroid::FromNativeGURL(env, info.url);
  ScopedJavaLocalRef<jstring> juser_agent =
      ConvertUTF8ToJavaString(env, info.user_agent);
  ScopedJavaLocalRef<jstring> jmime_type =
      ConvertUTF8ToJavaString(env, info.original_mime_type);
  ScopedJavaLocalRef<jstring> jcookie =
      ConvertUTF8ToJavaString(env, info.cookie);
  ScopedJavaLocalRef<jobject> jreferer =
      url::GURLAndroid::FromNativeGURL(env, info.referer);
  ScopedJavaLocalRef<jstring> jfile_name =
      base::android::ConvertUTF16ToJavaString(env, file_name);
  Java_DownloadController_enqueueAndroidDownloadManagerRequest(
      env, jurl, juser_agent, jfile_name, jmime_type, jcookie, jreferer);

  WebContents* web_contents = wc_getter.Run();
  CloseTabIfEmpty(web_contents, nullptr);
}

void DownloadController::OnDownloadStarted(DownloadItem* download_item) {
  // For dangerous downloads, we need to show the dangerous infobar before the
  // download can start.
  if (!download_item->IsDangerous() &&
      download_item->GetMimeType() == pdf::kPDFMimeType &&
      ShouldOpenPdfInline(download_item)) {
    content::WebContents* web_contents =
        content::DownloadItemUtils::GetWebContents(download_item);
    bool should_cancel_download = true;
    if (web_contents &&
        !NewNavigationObserver::GetInstance()->HasNewNavigation(web_contents)) {
      TabAndroid* tab = TabAndroid::FromWebContents(web_contents);
      if (tab) {
        JNIEnv* env = base::android::AttachCurrentThread();
        ScopedJavaLocalRef<jobject> j_item =
            DownloadManagerService::CreateJavaDownloadInfo(env, download_item);
        Java_DownloadController_onPdfDownloadStarted(env, tab->GetJavaObject(),
                                                     j_item);
        should_cancel_download = false;
      }
    }
    NewNavigationObserver::GetInstance()->StopObserving(web_contents);
    if (should_cancel_download) {
      ScheduleRemoveDownloadItem(download_item);
      download_item->RemoveObserver(this);
      return;
    }
  }

  // Register for updates to the DownloadItem.
  download_item->RemoveObserver(this);
  download_item->AddObserver(this);

  if (download::AutoResumptionHandler::Get())
    download::AutoResumptionHandler::Get()->OnDownloadStarted(download_item);

  ProfileKey* profile_key = GetProfileKey(download_item);
  if (!profile_key)
    return;

  DownloadOfflineContentProviderFactory::GetForKey(profile_key)
      ->OnDownloadStarted(download_item);

  OnDownloadUpdated(download_item);
}

void DownloadController::OnDownloadUpdated(DownloadItem* item) {
  if (item->IsTemporary() || item->IsTransient()) {
    // Only allow inline pdf file to proceed.
    if (item->GetMimeType() != pdf::kPDFMimeType ||
        !ShouldOpenPdfInline(item)) {
      return;
    }
  }

  if (item->IsDangerous() && (item->GetState() != DownloadItem::CANCELLED)) {
    // Dont't show notification for a dangerous download, as user can resume
    // the download after browser crash through notification.
    OnDangerousDownload(item);
    return;
  }

  if (item->GetState() == DownloadItem::COMPLETE) {
    if (ShouldShowAppVerificationPrompt(item)) {
      LogAppVerificationPromptToPrefs(item);
      app_verification_prompt_download_ = item;
      safe_browsing::SafeBrowsingApiHandlerBridge::GetInstance()
          .StartEnableVerifyApps(base::BindOnce(
              &DownloadController::EnableVerifyAppsDone,
              // base::Unretained is safe because `this` is a singleton.
              base::Unretained(this), item));
    } else if (app_verification_prompt_download_ != item) {
      OnDownloadComplete(item);
    }
  }
}

void DownloadController::OnDownloadDestroyed(download::DownloadItem* item) {
  item->RemoveObserver(this);
  if (app_verification_prompt_download_ == item) {
    app_verification_prompt_download_ = nullptr;
  }
}

void DownloadController::OnDangerousDownload(download::DownloadItem* item) {
  WebContents* web_contents = content::DownloadItemUtils::GetWebContents(item);
  if (!web_contents) {
    ScheduleRemoveDownloadItem(item);
    item->RemoveObserver(this);
    return;
  }

  ui::ViewAndroid* view_android =
      web_contents ? web_contents->GetNativeView() : nullptr;
  ui::WindowAndroid* window_android =
      view_android ? view_android->GetWindowAndroid() : nullptr;
  if (!dangerous_download_bridge_) {
    dangerous_download_bridge_ =
        std::make_unique<DangerousDownloadDialogBridge>();
  }
  dangerous_download_bridge_->Show(item, window_android);
}

void DownloadController::EnableVerifyAppsDone(
    download::DownloadItem* item,
    safe_browsing::VerifyAppsEnabledResult result) {
  base::UmaHistogramEnumeration(
      "SBClientDownload.AndroidAppVerificationPromptResult", result);

  if (app_verification_prompt_download_ != nullptr) {
    app_verification_prompt_download_ = nullptr;
    OnDownloadComplete(item);
  }
}

void DownloadController::OnDownloadComplete(download::DownloadItem* item) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_item =
      DownloadManagerService::CreateJavaDownloadInfo(env, item);
  // Multiple OnDownloadUpdated() notifications may be issued while the
  // download is in the COMPLETE state. Only handle one.
  item->RemoveObserver(this);
  bool is_download_safe = true;
  // Call onDownloadCompleted
  TabAndroid* tab = nullptr;
  if (base::FeatureList::IsEnabled(features::kAndroidOpenPdfInline)) {
    // Primary page of the WebContents have changed when showing the native
    // page, need to call GetOriginalWebContents() instead.
    content::WebContents* web_contents =
        content::DownloadItemUtils::GetOriginalWebContents(item);
    if (web_contents) {
      tab = TabAndroid::FromWebContents(web_contents);
    }
    download::DownloadItem::InsecureDownloadStatus status =
        GetInsecureDownloadStatusForDownload(
            Profile::FromBrowserContext(
                content::DownloadItemUtils::GetBrowserContext(item)),
            item->GetTargetFilePath(), item);
    is_download_safe =
        (status == download::DownloadItem::InsecureDownloadStatus::SAFE ||
         status == download::DownloadItem::InsecureDownloadStatus::VALIDATED);
  }
  Java_DownloadController_onDownloadCompleted(
      env, tab ? tab->GetJavaObject() : nullptr, j_item, is_download_safe);
}

void DownloadController::StartContextMenuDownload(
    const ContextMenuParams& params,
    WebContents* web_contents,
    bool is_link) {
  int process_id = web_contents->GetRenderViewHost()->GetProcess()->GetID();
  int routing_id = web_contents->GetRenderViewHost()->GetRoutingID();

  const content::WebContents::Getter& wc_getter(
      base::BindRepeating(&GetWebContents, process_id, routing_id));

  AcquireFileAccessPermission(
      wc_getter, base::BindOnce(&CreateContextMenuDownloadInternal, wc_getter,
                                params, is_link));
}

ProfileKey* DownloadController::GetProfileKey(DownloadItem* download_item) {
  Profile* profile = Profile::FromBrowserContext(
      content::DownloadItemUtils::GetBrowserContext(download_item));

  ProfileKey* profile_key;
  if (profile)
    profile_key = profile->GetProfileKey();
  else
    profile_key = ProfileKeyStartupAccessor::GetInstance()->profile_key();

  return profile_key;
}

bool DownloadController::ShouldShowAppVerificationPrompt(
    download::DownloadItem* item) {
  if (item->GetDangerType() != download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED) {
    return false;
  }

  if (app_verification_prompt_download_ != nullptr) {
    return false;
  }

  CleanupAppVerificationTimestamps(item);
  if (HasSeenTooManyAppVerificationPrompts(item)) {
    return false;
  }

  return true;
}
