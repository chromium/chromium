// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/android/downloads/offline_page_download_bridge.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chrome/android/chrome_jni_headers/OfflinePageDownloadBridge_jni.h"
#include "chrome/browser/android/profile_key_util.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/download/android/download_controller_base.h"
#include "chrome/browser/image_fetcher/image_decoder_impl.h"
#include "chrome/browser/offline_items_collection/offline_content_aggregator_factory.h"
#include "chrome/browser/offline_pages/android/downloads/offline_page_infobar_delegate.h"
#include "chrome/browser/offline_pages/android/downloads/offline_page_share_helper.h"
#include "chrome/browser/offline_pages/offline_page_mhtml_archiver.h"
#include "chrome/browser/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/offline_pages/offline_page_utils.h"
#include "chrome/browser/offline_pages/recent_tab_helper.h"
#include "chrome/browser/offline_pages/request_coordinator_factory.h"
#include "chrome/browser/offline_pages/visuals_decoder_impl.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/transition_manager/full_browser_transition_manager.h"
#include "components/download/public/common/download_url_parameters.h"
#include "components/offline_items_collection/core/offline_content_aggregator.h"
#include "components/offline_items_collection/core/offline_content_provider.h"
#include "components/offline_pages/core/background/request_coordinator.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/downloads/download_ui_adapter.h"
#include "components/offline_pages/core/offline_page_client_policy.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/offline_page_item_utils.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_request_utils.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/filename_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using ShareCallback =
    offline_items_collection::OfflineContentProvider::ShareCallback;

namespace offline_pages {
namespace android {

namespace {

void OnShareInfoRetrieved(std::unique_ptr<OfflinePageShareHelper>,
                          ShareCallback share_callback,
                          ShareResult result,
                          const ContentId& id,
                          std::unique_ptr<OfflineItemShareInfo> info) {
  // When |info| is null, the page URL will be used in sharing.
  if (result != ShareResult::kFileAccessPermissionDenied)
    std::move(share_callback).Run(id, std::move(info));

  // TODO(jianli, xingliu): When the permission request was denied by the user
  // and "Never ask again" was checked, we'd better show the permission update
  // infobar to remind the user. Currently the infobar only works for
  // ChromeActivities. We need to investigate how to make it work for other
  // activities.
}

class DownloadUIAdapterDelegate : public DownloadUIAdapter::Delegate {
 public:
  explicit DownloadUIAdapterDelegate(OfflinePageModel* model);

  // DownloadUIAdapter::Delegate
  bool IsVisibleInUI(const ClientId& client_id) override;
  void SetUIAdapter(DownloadUIAdapter* ui_adapter) override;
  void OpenItem(const OfflineItem& item,
                int64_t offline_id,
                offline_items_collection::LaunchLocation location) override;
  bool MaybeSuppressNotification(const std::string& origin,
                                 const ClientId& id) override;
  void GetShareInfoForItem(const ContentId& id,
                           ShareCallback share_callback) override;

 private:
  // Not owned, cached service pointer.
  OfflinePageModel* model_;
};

DownloadUIAdapterDelegate::DownloadUIAdapterDelegate(OfflinePageModel* model)
    : model_(model) {}

bool DownloadUIAdapterDelegate::IsVisibleInUI(const ClientId& client_id) {
  const std::string& name_space = client_id.name_space;
  return GetPolicy(name_space).is_supported_by_download &&
         base::IsValidGUID(client_id.id);
}

void DownloadUIAdapterDelegate::SetUIAdapter(DownloadUIAdapter* ui_adapter) {}

void DownloadUIAdapterDelegate::OpenItem(
    const OfflineItem& item,
    int64_t offline_id,
    offline_items_collection::LaunchLocation location) {
  JNIEnv* env = AttachCurrentThread();
  Java_OfflinePageDownloadBridge_openItem(
      env, ConvertUTF8ToJavaString(env, item.page_url.spec()), offline_id,
      static_cast<int>(location),
      offline_pages::ShouldOfflinePagesInDownloadHomeOpenInCct());
}

bool DownloadUIAdapterDelegate::MaybeSuppressNotification(
    const std::string& origin,
    const ClientId& id) {
  // Do not suppress notification if chrome.
  if (origin == "" || !IsOfflinePagesSuppressNotificationsEnabled())
    return false;
  JNIEnv* env = AttachCurrentThread();
  return Java_OfflinePageDownloadBridge_maybeSuppressNotification(
      env, ConvertUTF8ToJavaString(env, origin),
      ConvertUTF8ToJavaString(env, id.id));
}

void DownloadUIAdapterDelegate::GetShareInfoForItem(
    const ContentId& id,
    ShareCallback share_callback) {
  auto share_helper = std::make_unique<OfflinePageShareHelper>(model_);
  share_helper->GetShareInfo(
      id, base::BindOnce(&OnShareInfoRetrieved, std::move(share_helper),
                         std::move(share_callback)));
}

// TODO(dewittj): Move to Download UI Adapter.
content::WebContents* GetWebContentsFromJavaTab(
    const ScopedJavaGlobalRef<jobject>& j_tab_ref) {
  JNIEnv* env = AttachCurrentThread();
  TabAndroid* tab = TabAndroid::GetNativeTab(env, j_tab_ref);
  if (!tab)
    return nullptr;

  return tab->web_contents();
}

void SavePageIfNotNavigatedAway(const GURL& url,
                                const GURL& original_url,
                                const ScopedJavaGlobalRef<jobject>& j_tab_ref,
                                const std::string& origin) {
  content::WebContents* web_contents = GetWebContentsFromJavaTab(j_tab_ref);
  if (!web_contents)
    return;

  // This ignores fragment differences in URLs, bails out only if tab has
  // navigated away and not just scrolled to a fragment.
  GURL current_url = web_contents->GetLastCommittedURL();
  if (!EqualsIgnoringFragment(current_url, url))
    return;

  offline_pages::ClientId client_id;
  client_id.name_space = offline_pages::kDownloadNamespace;
  client_id.id = base::GenerateGUID();
  int64_t request_id = OfflinePageModel::kInvalidOfflineId;

  // Post disabled request before passing the download task to the tab helper.
  // This will keep the request persisted in case Chrome is evicted from RAM
  // or closed by the user.
  // Note: the 'disabled' status is not persisted (stored in memory) so it
  // automatically resets if Chrome is re-started.
  offline_pages::RequestCoordinator* request_coordinator =
      offline_pages::RequestCoordinatorFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());
  if (request_coordinator) {
    offline_pages::RequestCoordinator::SavePageLaterParams params;
    params.url = current_url;
    params.client_id = client_id;
    params.availability =
        RequestCoordinator::RequestAvailability::DISABLED_FOR_OFFLINER;
    params.original_url = original_url;
    params.request_origin = origin;
    request_id = request_coordinator->SavePageLater(params, base::DoNothing());
  } else {
    DVLOG(1) << "SavePageIfNotNavigatedAway has no valid coordinator.";
  }

  // Pass request_id to the current tab's helper to attempt download right from
  // the tab. If unsuccessful, it'll enable the already-queued request for
  // background offliner. Same will happen if Chrome is terminated since
  // 'disabled' status of the request is RAM-stored info.
  offline_pages::RecentTabHelper* tab_helper =
      RecentTabHelper::FromWebContents(web_contents);
  if (!tab_helper) {
    if (request_id != OfflinePageModel::kInvalidOfflineId) {
      offline_pages::RequestCoordinator* request_coordinator =
          offline_pages::RequestCoordinatorFactory::GetForBrowserContext(
              web_contents->GetBrowserContext());
      if (request_coordinator)
        request_coordinator->EnableForOffliner(request_id, client_id);
      else
        DVLOG(1) << "SavePageIfNotNavigatedAway has no valid coordinator.";
    }
    return;
  }
  tab_helper->ObserveAndDownloadCurrentPage(client_id, request_id, origin);

  OfflinePageDownloadBridge::ShowDownloadingToast();
}

void DuplicateCheckDone(const GURL& url,
                        const GURL& original_url,
                        const ScopedJavaGlobalRef<jobject>& j_tab_ref,
                        const std::string& origin,
                        OfflinePageUtils::DuplicateCheckResult result) {
  if (result == OfflinePageUtils::DuplicateCheckResult::NOT_FOUND) {
    SavePageIfNotNavigatedAway(url, original_url, j_tab_ref, origin);
    return;
  }

  content::WebContents* web_contents = GetWebContentsFromJavaTab(j_tab_ref);
  if (!web_contents)
    return;

  bool duplicate_request_exists =
      result == OfflinePageUtils::DuplicateCheckResult::DUPLICATE_REQUEST_FOUND;
  OfflinePageInfoBarDelegate::Create(
      base::Bind(&SavePageIfNotNavigatedAway, url, original_url, j_tab_ref,
                 origin),
      url, duplicate_request_exists, web_contents);
}


content::WebContents* GetWebContentsByFrameID(int render_process_id,
                                              int render_frame_id) {
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);
  if (!render_frame_host)
    return NULL;
  return content::WebContents::FromRenderFrameHost(render_frame_host);
}

content::WebContents::Getter GetWebContentsGetter(
    content::WebContents* web_contents) {
  // The FrameTreeNode ID should be used to access the WebContents.
  int frame_tree_node_id = web_contents->GetMainFrame()->GetFrameTreeNodeId();
  if (frame_tree_node_id != -1) {
    return base::Bind(content::WebContents::FromFrameTreeNodeId,
                      frame_tree_node_id);
  }

  // In other cases, use the RenderProcessHost ID + RenderFrameHost ID to get
  // the WebContents.
  return base::Bind(&GetWebContentsByFrameID,
                    web_contents->GetMainFrame()->GetProcess()->GetID(),
                    web_contents->GetMainFrame()->GetRoutingID());
}

void DownloadAsFile(content::WebContents* web_contents, const GURL& url) {
  content::DownloadManager* dlm = content::BrowserContext::GetDownloadManager(
      web_contents->GetBrowserContext());
  std::unique_ptr<download::DownloadUrlParameters> dl_params(
      content::DownloadRequestUtils::CreateDownloadForWebContentsMainFrame(
          web_contents, url,
          TRAFFIC_ANNOTATION_WITHOUT_PROTO("Offline pages download file")));

  content::NavigationEntry* entry =
      web_contents->GetController().GetLastCommittedEntry();
  // |entry| should not be null since otherwise an empty URL is returned from
  // calling GetLastCommittedURL and we should bail out earlier.
  DCHECK(entry);
  content::Referrer referrer =
      content::Referrer::SanitizeForRequest(url, entry->GetReferrer());
  dl_params->set_referrer(referrer.url);
  dl_params->set_referrer_policy(
      content::Referrer::ReferrerPolicyForUrlRequest(referrer.policy));
  dl_params->set_prompt(false);
  dl_params->set_download_source(download::DownloadSource::OFFLINE_PAGE);
  dlm->DownloadUrl(std::move(dl_params));
}

void OnOfflinePageAcquireFileAccessPermissionDone(
    const content::WebContents::Getter& web_contents_getter,
    const ScopedJavaGlobalRef<jobject>& j_tab_ref,
    const std::string& origin,
    bool granted) {
  if (!granted)
    return;

  content::WebContents* web_contents = web_contents_getter.Run();
  if (!web_contents)
    return;

  GURL url = web_contents->GetLastCommittedURL();
  if (url.is_empty())
    return;

  // If the page is not a HTML page, route to DownloadManager.
  if (!offline_pages::OfflinePageUtils::CanDownloadAsOfflinePage(
          url, web_contents->GetContentsMimeType())) {
    DownloadAsFile(web_contents, url);
    return;
  }

  // Otherwise, save the HTML page as archive.
  GURL original_url =
      offline_pages::OfflinePageUtils::GetOriginalURLFromWebContents(
          web_contents);
  OfflinePageUtils::CheckDuplicateDownloads(
      chrome::GetBrowserContextRedirectedInIncognito(
          web_contents->GetBrowserContext()),
      url,
      base::Bind(&DuplicateCheckDone, url, original_url, j_tab_ref, origin));
}

void InitializeBackendOnProfileCreated(Profile* profile) {
  // Even if |profile| is incognito, use the regular one since downloads are
  // shared between them.
  profile = profile->GetOriginalProfile();
  OfflinePageModel* offline_page_model =
      OfflinePageModelFactory::GetForBrowserContext(profile);
  DCHECK(offline_page_model);

  DownloadUIAdapter* adapter =
      DownloadUIAdapter::FromOfflinePageModel(offline_page_model);

  if (!adapter) {
    RequestCoordinator* request_coordinator =
        RequestCoordinatorFactory::GetForBrowserContext(profile);
    DCHECK(request_coordinator);
    offline_items_collection::OfflineContentAggregator* aggregator =
        OfflineContentAggregatorFactory::GetForKey(profile->GetProfileKey());
    DCHECK(aggregator);
    adapter = new DownloadUIAdapter(
        aggregator, offline_page_model, request_coordinator,
        std::make_unique<VisualsDecoderImpl>(
            std::make_unique<ImageDecoderImpl>()),
        std::make_unique<DownloadUIAdapterDelegate>(offline_page_model));
    DownloadUIAdapter::AttachToOfflinePageModel(base::WrapUnique(adapter),
                                                offline_page_model);
  }
}

}  // namespace

OfflinePageDownloadBridge::OfflinePageDownloadBridge(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj)
    : weak_java_ref_(env, obj) {}

OfflinePageDownloadBridge::~OfflinePageDownloadBridge() {}

void OfflinePageDownloadBridge::Destroy(JNIEnv* env,
                                        const JavaParamRef<jobject>&) {
  delete this;
}

void JNI_OfflinePageDownloadBridge_StartDownload(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_tab,
    const JavaParamRef<jstring>& j_origin) {
  TabAndroid* tab = TabAndroid::GetNativeTab(env, j_tab);
  if (!tab)
    return;

  content::WebContents* web_contents = tab->web_contents();
  if (!web_contents)
    return;

  std::string origin = ConvertJavaStringToUTF8(env, j_origin);
  ScopedJavaGlobalRef<jobject> j_tab_ref(env, j_tab);

  // Ensure that the storage permission is granted since the target file
  // is going to be placed in the public directory.
  content::WebContents::Getter web_contents_getter =
      GetWebContentsGetter(web_contents);
  DownloadControllerBase::Get()->AcquireFileAccessPermission(
      web_contents_getter,
      base::Bind(&OnOfflinePageAcquireFileAccessPermissionDone,
                 web_contents_getter, j_tab_ref, origin));
}

static jlong JNI_OfflinePageDownloadBridge_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  ProfileKey* key = ::android::GetLastUsedProfileKey();
  FullBrowserTransitionManager::Get()->RegisterCallbackOnProfileCreation(
      key, base::BindOnce(&InitializeBackendOnProfileCreated));

  return reinterpret_cast<jlong>(new OfflinePageDownloadBridge(env, obj));
}

// static
void OfflinePageDownloadBridge::ShowDownloadingToast() {
  JNIEnv* env = AttachCurrentThread();
  Java_OfflinePageDownloadBridge_showDownloadingToast(env);
}

}  // namespace android
}  // namespace offline_pages
