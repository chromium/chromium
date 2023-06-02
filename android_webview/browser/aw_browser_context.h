// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_BROWSER_CONTEXT_H_
#define ANDROID_WEBVIEW_BROWSER_AW_BROWSER_CONTEXT_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "android_webview/browser/aw_contents_origin_matcher.h"
#include "android_webview/browser/aw_permission_manager.h"
#include "android_webview/browser/aw_ssl_host_state_delegate.h"
#include "android_webview/browser/network_service/aw_proxy_config_monitor.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "components/keyed_service/core/simple_factory_key.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/visitedlink/browser/visitedlink_delegate.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/zoom_level_delegate.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom-forward.h"

class GURL;
class PrefService;

namespace autofill {
class AutocompleteHistoryManager;
}

namespace content {
class ClientHintsControllerDelegate;
class ResourceContext;
class SSLHostStateDelegate;
class WebContents;
}

namespace download {
class InProgressDownloadManager;
}

namespace visitedlink {
class VisitedLinkWriter;
}

namespace android_webview {

class AwContentsOriginMatcher;
class AwFormDatabaseService;
class AwQuotaManagerBridge;
class CookieManager;

class AwBrowserContext : public content::BrowserContext,
                         public visitedlink::VisitedLinkDelegate {
 public:
  AwBrowserContext();

  AwBrowserContext(const AwBrowserContext&) = delete;
  AwBrowserContext& operator=(const AwBrowserContext&) = delete;

  ~AwBrowserContext() override;

  // Currently only one instance per process is supported.
  static AwBrowserContext* GetDefault();

  // Convenience method to returns the AwBrowserContext corresponding to the
  // given WebContents.
  static AwBrowserContext* FromWebContents(
      content::WebContents* web_contents);

  base::FilePath GetCacheDir();
  base::FilePath GetPrefStorePath();
  base::FilePath GetCookieStorePath();
  static base::FilePath GetContextStoragePath();

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Get the list of authentication schemes to support.
  static std::vector<std::string> GetAuthSchemes();

  // These methods map to Add methods in visitedlink::VisitedLinkWriter.
  void AddVisitedURLs(const std::vector<GURL>& urls);

  AwQuotaManagerBridge* GetQuotaManagerBridge();
  jlong GetQuotaManagerBridge(JNIEnv* env);
  void SetWebLayerRunningInSameProcess(JNIEnv* env);

  AwFormDatabaseService* GetFormDatabaseService();
  autofill::AutocompleteHistoryManager* GetAutocompleteHistoryManager();
  CookieManager* GetCookieManager();

  // TODO(amalova): implement for non-default browser context
  bool IsDefaultBrowserContext() { return true; }

  base::android::ScopedJavaLocalRef<jobjectArray>
  UpdateServiceWorkerXRequestedWithAllowListOriginMatcher(
      JNIEnv* env,
      const base::android::JavaParamRef<jobjectArray>& rules);

  // content::BrowserContext implementation.
  base::FilePath GetPath() override;
  bool IsOffTheRecord() override;
  content::ResourceContext* GetResourceContext() override;
  content::DownloadManagerDelegate* GetDownloadManagerDelegate() override;
  content::BrowserPluginGuestManager* GetGuestManager() override;
  storage::SpecialStoragePolicy* GetSpecialStoragePolicy() override;
  content::PlatformNotificationService* GetPlatformNotificationService()
      override;
  content::PushMessagingService* GetPushMessagingService() override;
  content::StorageNotificationService* GetStorageNotificationService() override;
  content::SSLHostStateDelegate* GetSSLHostStateDelegate() override;
  AwPermissionManager* GetPermissionControllerDelegate() override;
  content::ClientHintsControllerDelegate* GetClientHintsControllerDelegate()
      override;
  content::BackgroundFetchDelegate* GetBackgroundFetchDelegate() override;
  content::BackgroundSyncController* GetBackgroundSyncController() override;
  content::BrowsingDataRemoverDelegate* GetBrowsingDataRemoverDelegate()
      override;
  content::ReduceAcceptLanguageControllerDelegate*
  GetReduceAcceptLanguageControllerDelegate() override;
  std::unique_ptr<download::InProgressDownloadManager>
  RetrieveInProgressDownloadManager() override;
  content::OriginTrialsControllerDelegate* GetOriginTrialsControllerDelegate()
      override;
  std::unique_ptr<content::ZoomLevelDelegate> CreateZoomLevelDelegate(
      const base::FilePath& partition_path) override;

  // visitedlink::VisitedLinkDelegate implementation.
  void RebuildTable(const scoped_refptr<URLEnumerator>& enumerator) override;

  PrefService* GetPrefService() const { return user_pref_service_.get(); }

  void SetExtendedReportingAllowed(bool allowed);

  void ConfigureNetworkContextParams(
      bool in_memory,
      const base::FilePath& relative_partition_path,
      network::mojom::NetworkContextParams* network_context_params,
      cert_verifier::mojom::CertVerifierCreationParams*
          cert_verifier_creation_params);

  base::android::ScopedJavaLocalRef<jobject> GetJavaBrowserContext();

  void ClearPersistentOriginTrialStorageForTesting(JNIEnv* env);

  jboolean HasFormData(JNIEnv* env);
  void ClearFormData(JNIEnv* env);

  scoped_refptr<AwContentsOriginMatcher> service_worker_xrw_allowlist_matcher();

  void SetExtraHeaders(const GURL& url, const std::string& headers);
  std::string GetExtraHeaders(const GURL& url);

 private:
  void CreateUserPrefService();
  void MigrateLocalStatePrefs();

  // The file path where data for this context is persisted.
  base::FilePath context_storage_path_;

  scoped_refptr<AwQuotaManagerBridge> quota_manager_bridge_;
  std::unique_ptr<AwFormDatabaseService> form_database_service_;
  std::unique_ptr<autofill::AutocompleteHistoryManager>
      autocomplete_history_manager_;

  std::unique_ptr<visitedlink::VisitedLinkWriter> visitedlink_writer_;
  std::unique_ptr<content::ResourceContext> resource_context_;

  std::unique_ptr<PrefService> user_pref_service_;
  std::unique_ptr<AwSSLHostStateDelegate> ssl_host_state_delegate_;
  std::unique_ptr<AwPermissionManager> permission_manager_;
  std::unique_ptr<content::ClientHintsControllerDelegate>
      client_hints_controller_delegate_;
  std::unique_ptr<content::OriginTrialsControllerDelegate>
      origin_trials_controller_delegate_;

  SimpleFactoryKey simple_factory_key_;

  scoped_refptr<AwContentsOriginMatcher> service_worker_xrw_allowlist_matcher_;

  std::map<std::string, std::string> extra_headers_;

  base::android::ScopedJavaGlobalRef<jobject> obj_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_BROWSER_CONTEXT_H_
