// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_BROWSER_CONTEXT_H_
#define ANDROID_WEBVIEW_BROWSER_AW_BROWSER_CONTEXT_H_

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "android_webview/browser/aw_contents_io_thread_client.h"
#include "android_webview/browser/aw_contents_origin_matcher.h"
#include "android_webview/browser/aw_context_permissions_delegate.h"
#include "android_webview/browser/aw_permission_manager.h"
#include "android_webview/browser/aw_ssl_host_state_delegate.h"
#include "android_webview/browser/file_system_access/aw_file_system_access_permission_context.h"
#include "android_webview/browser/network_service/aw_proxy_config_monitor.h"
#include "android_webview/browser/prefetch/aw_prefetch_manager.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/simple_factory_key.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/visitedlink/browser/visitedlink_delegate.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/zoom_level_delegate.h"
#include "net/http/http_request_headers.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom-forward.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-shared.h"

class GURL;
class PrefService;

namespace content {
class ClientHintsControllerDelegate;
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

class AwBrowserContextIoThreadHandle;
class AwContentsOriginMatcher;
class AwFormDatabaseService;
class AwQuotaManagerBridge;
class CookieManager;

// The maximum number of prerendering allowed for this BrowserContext.
inline constexpr int MAX_ALLOWED_PRERENDERING_COUNT = 3;

// Lifetime: Profile
class AwBrowserContext : public content::BrowserContext,
                         public visitedlink::VisitedLinkDelegate,
                         public AwContextPermissionsDelegate {
 public:
  explicit AwBrowserContext(std::string name,
                            base::FilePath relative_path,
                            bool is_default);

  AwBrowserContext(const AwBrowserContext&) = delete;
  AwBrowserContext& operator=(const AwBrowserContext&) = delete;

  ~AwBrowserContext() override;

  // Return the default context. The default context must be initialized.
  static AwBrowserContext* GetDefault();

  // Convenience method to returns the AwBrowserContext corresponding to the
  // given WebContents.
  static AwBrowserContext* FromWebContents(
      content::WebContents* web_contents);

  base::FilePath GetHttpCachePath();
  base::FilePath GetPrefStorePath();
  base::FilePath GetCookieStorePath();
  static base::FilePath BuildStoragePath(const base::FilePath& relative_path);

  // Set up the paths and other data storage for a new context.
  static void PrepareNewContext(const base::FilePath& relative_path);
  // Delete all files and other stored data for a given context.
  static void DeleteContext(const base::FilePath& relative_path);
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Get the list of authentication schemes to support.
  static std::vector<std::string> GetAuthSchemes();

  // These methods map to Add methods in visitedlink::VisitedLinkWriter.
  void AddVisitedURLs(const std::vector<GURL>& urls);

  AwQuotaManagerBridge* GetQuotaManagerBridge();
  jlong GetQuotaManagerBridge(JNIEnv* env);

  AwFormDatabaseService* GetFormDatabaseService();
  CookieManager* GetCookieManager();

  bool IsDefaultBrowserContext() const;

  base::android::ScopedJavaLocalRef<jobjectArray>
  UpdateServiceWorkerXRequestedWithAllowListOriginMatcher(
      JNIEnv* env,
      const base::android::JavaParamRef<jobjectArray>& rules);
  void SetServiceWorkerIoThreadClient(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& io_thread_client);

  int AllowedPrerenderingCount() const;
  void SetAllowedPrerenderingCount(JNIEnv* const env, int allowed_count);
  void WarmUpSpareRenderer(JNIEnv* const env);

  // content::BrowserContext implementation.
  base::FilePath GetPath() override;
  bool IsOffTheRecord() override;
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
  content::FileSystemAccessPermissionContext*
  GetFileSystemAccessPermissionContext() override;
  content::ReduceAcceptLanguageControllerDelegate*
  GetReduceAcceptLanguageControllerDelegate() override;
  std::unique_ptr<download::InProgressDownloadManager>
  RetrieveInProgressDownloadManager() override;
  content::OriginTrialsControllerDelegate* GetOriginTrialsControllerDelegate()
      override;
  std::unique_ptr<content::ZoomLevelDelegate> CreateZoomLevelDelegate(
      const base::FilePath& partition_path) override;
  std::string GetExtraHeadersForUrl(const GURL& url) override;

  // visitedlink::VisitedLinkDelegate implementation.
  void RebuildTable(const scoped_refptr<URLEnumerator>& enumerator) override;
  void BuildVisitedLinkTable(
      const scoped_refptr<VisitedLinkEnumerator>& enumerator) override;

  // android_webview::AwContextPermissionsDelegate implementation.
  blink::mojom::PermissionStatus GetGeolocationPermission(
      const GURL& origin) const override;

  // Returns the default "Accept Language" header before WebView has access
  // to the user preferred locale.
  std::string GetDefaultAcceptLanguageHeader();

  mojo::PendingRemote<network::mojom::URLLoaderFactory>
  CreateURLLoaderFactory();

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

  scoped_refptr<AwContentsOriginMatcher> service_worker_xrw_allowlist_matcher();

  void SetExtraHeaders(const GURL& url, const std::string& headers);

 private:
  friend class AwBrowserContextIoThreadHandle;
  void CreateUserPrefService();
  void MigrateLocalStatePrefs();

  // Return the IO thread client for this browser context that should be used
  // by service workers. This method should never be called except by
  // AwBrowserContextIoThreadHandle#GetServiceWorkerIoThreadClient().
  std::unique_ptr<AwContentsIoThreadClient>
  GetServiceWorkerIoThreadClientThreadSafe();

  const std::string name_;
  const base::FilePath relative_path_;
  const bool is_default_;

  // The file path where data for this context is persisted.
  base::FilePath context_storage_path_;
  base::FilePath http_cache_path_;

  scoped_refptr<AwQuotaManagerBridge> quota_manager_bridge_;

  // Cleans up the database tables created by the AwFormDatabaseService
  // until M132.
  // TODO(crbug.com/390473673): Remove after M143.
  std::unique_ptr<AwFormDatabaseService> form_database_service_;

  std::unique_ptr<visitedlink::VisitedLinkWriter> visitedlink_writer_;

  std::unique_ptr<PrefService> user_pref_service_;
  std::unique_ptr<AwSSLHostStateDelegate> ssl_host_state_delegate_;
  std::unique_ptr<AwPermissionManager> permission_manager_;
  std::unique_ptr<content::ClientHintsControllerDelegate>
      client_hints_controller_delegate_;
  std::unique_ptr<content::OriginTrialsControllerDelegate>
      origin_trials_controller_delegate_;

  AwFileSystemAccessPermissionContext fsa_permission_context_;
  SimpleFactoryKey simple_factory_key_;

  scoped_refptr<AwContentsOriginMatcher> service_worker_xrw_allowlist_matcher_;

  std::map<std::string, std::string> extra_headers_;

  base::android::ScopedJavaGlobalRef<jobject> obj_;

  // For non-default profiles, the context owns its CookieManager in
  // cookie_manager_ (and it will not be null after construction). But if this
  // is the default profile then cookie_manager_ will be null, as the default
  // context does not own the default cookie manager (it is instead obtained via
  // CookieManager::GetDefaultInstance()).
  //
  // In generally, use GetCookieManager() rather than using this directly.
  std::unique_ptr<CookieManager> cookie_manager_;

  std::unique_ptr<AwPrefetchManager> prefetch_manager_;

  // The IO thread client that should be used by service workers.
  base::android::ScopedJavaGlobalRef<jobject> sw_io_thread_client_;

  // The maximum number of concurrent prerendering attempts that can be
  // triggered by AwContents::StartPrerendering().
  int allowed_prerendering_count_ = 2;

  // Enables usage of net::StaleHostResolver. This will not be applied to any
  // in-flight requests, only applied to the requests made afterwards. It should
  // be enabled before making any requests.
  bool enable_stale_dns_ = false;

  base::WeakPtrFactory<AwBrowserContext> weak_method_factory_{this};
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_BROWSER_CONTEXT_H_
