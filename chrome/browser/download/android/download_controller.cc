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
#include "base/lazy_instance.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/synchronization/lock.h"
#include "chrome/android/chrome_jni_headers/DownloadController_jni.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/profile_key_startup_accessor.h"
#include "chrome/browser/android/profile_key_util.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/download/android/dangerous_download_infobar_delegate.h"
#include "chrome/browser/download/android/download_manager_service.h"
#include "chrome/browser/download/android/download_utils.h"
#include "chrome/browser/download/download_offline_content_provider.h"
#include "chrome/browser/download/download_offline_content_provider_factory.h"
#include "chrome/browser/download/download_stats.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/offline_pages/android/offline_page_bridge.h"
#include "chrome/browser/permissions/permission_update_infobar_delegate_android.h"
#include "chrome/browser/permissions/permission_update_message_controller_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/grit/branded_strings.h"
#include "components/download/content/public/context_menu_download.h"
#include "components/download/public/common/auto_resumption_handler.h"
#include "components/download/public/common/download_features.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/messages/android/messages_feature.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "net/base/filename_util.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/base/device_form_factor.h"
#include "ui/base/page_transition_types.h"
#include "url/android/gurl_android.h"

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
    DownloadController::RecordStoragePermission(
        DownloadController::StoragePermissionType::
            STORAGE_PERMISSION_NO_WEB_CONTENTS);
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

    if (messages::IsPermissionUpdateMessagesUiEnabled()) {
      PermissionUpdateMessageController::CreateForWebContents(web_contents);
      PermissionUpdateMessageController::FromWebContents(web_contents)
          ->ShowMessage(permissions, IDR_ANDORID_MESSAGE_PERMISSION_STORAGE,
                        IDS_MESSAGE_MISSING_STORAGE_ACCESS_PERMISSION_TITLE,
                        IDS_MESSAGE_STORAGE_ACCESS_PERMISSION_TEXT,
                        std::move(cb));
    } else {
      PermissionUpdateInfoBarDelegate::Create(
          web_contents, permissions,
          IDS_MISSING_STORAGE_PERMISSION_DOWNLOAD_EDUCATION_TEXT,
          std::move(cb));
    }
    return;
  }

  std::move(cb).Run(granted);
}

void OnStoragePermissionDecided(
    DownloadControllerBase::AcquireFileAccessPermissionCallback cb,
    bool granted) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (granted) {
    DownloadController::RecordStoragePermission(
        DownloadController::StoragePermissionType::STORAGE_PERMISSION_GRANTED);
  } else {
    DownloadController::RecordStoragePermission(
        DownloadController::StoragePermissionType::STORAGE_PERMISSION_DENIED);
  }

  std::move(cb).Run(granted);
}

}  // namespace

static void JNI_DownloadController_OnAcquirePermissionResult(
    JNIEnv* env,
    jlong callback_id,
    jboolean granted,
    const JavaParamRef<jstring>& jpermission_to_update) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(callback_id);

  if (!DownloadController::GetInstance()
           ->validator()
           ->ValidateAndClearJavaCallback(callback_id)) {
    return;
  }

  std::string permission_to_update;
  if (jpermission_to_update) {
    permission_to_update =
        base::android::ConvertJavaStringToUTF8(env, jpermission_to_update);
  }
  // Convert java long long int to c++ pointer, take ownership.
  std::unique_ptr<DownloadController::AcquirePermissionCallback> cb(
      reinterpret_cast<DownloadController::AcquirePermissionCallback*>(
          callback_id));
  std::move(*cb).Run(granted, permission_to_update);
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
void DownloadController::RecordStoragePermission(StoragePermissionType type) {
  UMA_HISTOGRAM_ENUMERATION("MobileDownload.StoragePermission", type,
                            STORAGE_PERMISSION_MAX);
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

  int tab_index = -1;
  for (int index = 0; index < tab_model->GetTabCount(); ++index) {
    if (web_contents == tab_model->GetWebContentsAt(index)) {
      tab_index = index;
      break;
    }
  }

  if (tab_index == -1)
    return;

  // Closing an empty page on external app download leaves a bad user experience
  // as user don't know whether a download is kicked off, or if Chrome just
  // ignores the URL. Show the download page instead.
  if (base::FeatureList::IsEnabled(
          chrome::android::kDownloadHomeForExternalApp) &&
      !base::FeatureList::IsEnabled(chrome::android::kChromeNewDownloadTab) &&
      tab_model->GetTabAt(tab_index)->GetLaunchType() ==
          static_cast<int>(TabModel::TabLaunchType::FROM_EXTERNAL_APP)) {
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
  tab_model->CloseTabAt(tab_index);
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
    RecordStoragePermission(
        StoragePermissionType::STORAGE_PERMISSION_REQUESTED);
    RecordStoragePermission(
        StoragePermissionType::STORAGE_PERMISSION_NO_ACTION_NEEDED);
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(cb), true));
    return;
  }

  RecordStoragePermission(StoragePermissionType::STORAGE_PERMISSION_REQUESTED);
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

void DownloadController::AboutToResumeDownload(DownloadItem* download_item) {
  download_item->RemoveObserver(this);
  download_item->AddObserver(this);

  // If a download is resumed from an interrupted state, record its strong
  // validators so we know whether the resumption causes a restart.
  if (download_item->GetState() == DownloadItem::IN_PROGRESS ||
      download_item->GetLastReason() ==
          download::DOWNLOAD_INTERRUPT_REASON_NONE) {
    return;
  }
  if (download_item->GetETag().empty() &&
      download_item->GetLastModifiedTime().empty()) {
    return;
  }
  strong_validators_map_.emplace(
      download_item->GetGuid(),
      std::make_pair(download_item->GetETag(),
                     download_item->GetLastModifiedTime()));
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
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!download_item->IsDangerous())
    Java_DownloadController_onDownloadStarted(env);

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
  if (item->IsTemporary() || item->IsTransient())
    return;

  if (item->IsDangerous() && (item->GetState() != DownloadItem::CANCELLED)) {
    // Dont't show notification for a dangerous download, as user can resume
    // the download after browser crash through notification.
    OnDangerousDownload(item);
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_item =
      DownloadManagerService::CreateJavaDownloadInfo(env, item);
  switch (item->GetState()) {
    case DownloadItem::IN_PROGRESS: {
      Java_DownloadController_onDownloadUpdated(env, j_item);
      break;
    }
    case DownloadItem::COMPLETE:
      strong_validators_map_.erase(item->GetGuid());
      // Multiple OnDownloadUpdated() notifications may be issued while the
      // download is in the COMPLETE state. Only handle one.
      item->RemoveObserver(this);

      // Call onDownloadCompleted
      Java_DownloadController_onDownloadCompleted(env, j_item);
      break;
    case DownloadItem::CANCELLED:
      strong_validators_map_.erase(item->GetGuid());
      Java_DownloadController_onDownloadCancelled(env, j_item);
      break;
    case DownloadItem::INTERRUPTED:
      if (item->IsDone())
        strong_validators_map_.erase(item->GetGuid());
      // When device loses/changes network, we get a NETWORK_TIMEOUT,
      // NETWORK_FAILED or NETWORK_DISCONNECTED error. Download should auto
      // resume in this case.
      Java_DownloadController_onDownloadInterrupted(
          env, j_item, IsInterruptedDownloadAutoResumable(item));
      break;
    case DownloadItem::MAX_DOWNLOAD_STATE:
      NOTREACHED();
  }
}

void DownloadController::OnDangerousDownload(DownloadItem* item) {
  WebContents* web_contents = content::DownloadItemUtils::GetWebContents(item);
  if (!web_contents) {
    auto download_manager_getter = std::make_unique<DownloadManagerGetter>(
        content::DownloadItemUtils::GetBrowserContext(item)
            ->GetDownloadManager());
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&RemoveDownloadItem, std::move(download_manager_getter),
                       item->GetGuid()));
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

bool DownloadController::IsInterruptedDownloadAutoResumable(
    download::DownloadItem* download_item) {
  if (!download_item->GetURL().SchemeIsHTTPOrHTTPS())
    return false;
  static int size_limit = DownloadUtils::GetAutoResumptionSizeLimit();
  bool exceeds_size_limit = download_item->GetReceivedBytes() > size_limit;
  std::string etag = download_item->GetETag();
  std::string last_modified = download_item->GetLastModifiedTime();

  if (exceeds_size_limit && etag.empty() && last_modified.empty() &&
      !base::FeatureList::IsEnabled(
          download::features::
              kAllowDownloadResumptionWithoutStrongValidators)) {
    return false;
  }

  // If the download has strong validators, but it caused a restart, stop auto
  // resumption as the server may always send new strong validators on
  // resumption.
  auto strong_validator = strong_validators_map_.find(download_item->GetGuid());
  if (strong_validator != strong_validators_map_.end()) {
    if (exceeds_size_limit &&
        (strong_validator->second.first != etag ||
         strong_validator->second.second != last_modified)) {
      return false;
    }
  }

  int interrupt_reason = download_item->GetLastReason();
  DCHECK_NE(interrupt_reason, download::DOWNLOAD_INTERRUPT_REASON_NONE);
  return interrupt_reason ==
             download::DOWNLOAD_INTERRUPT_REASON_NETWORK_TIMEOUT ||
         interrupt_reason ==
             download::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED ||
         interrupt_reason ==
             download::DOWNLOAD_INTERRUPT_REASON_NETWORK_DISCONNECTED;
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
