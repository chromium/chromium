// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_CONTENT_BROWSER_CLIENT_H_
#define CHROME_BROWSER_CHROME_CONTENT_BROWSER_CLIENT_H_

#include <stddef.h>

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_service.h"
#include "chrome/browser/metrics/chrome_feature_list_creator.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/previews_state.h"
#include "content/public/common/resource_type.h"
#include "extensions/buildflags/buildflags.h"
#include "media/media_buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"

class ChromeContentBrowserClientParts;
class PrefRegistrySimple;

namespace base {
class CommandLine;
}

namespace blink {
namespace mojom {
class WindowFeatures;
class WebUsbService;
}
}

namespace content {
class BrowserContext;
class QuotaPermissionContext;
}

namespace data_reduction_proxy {
class DataReductionProxyData;
}  // namespace data_reduction_proxy

namespace previews {
class PreviewsDecider;
class PreviewsUserData;
}  // namespace previews

namespace safe_browsing {
class SafeBrowsingService;
class UrlCheckerDelegate;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace version_info {
enum class Channel;
}

namespace url {
class Origin;
}

class ChromeContentBrowserClient : public content::ContentBrowserClient {
 public:
  explicit ChromeContentBrowserClient(
      ChromeFeatureListCreator* chrome_feature_list_creator = nullptr);
  ~ChromeContentBrowserClient() override;

  // TODO(https://crbug.com/787567): This file is about calls from content/ out
  // to chrome/ to get values or notify about events, but both of these
  // functions are from chrome/ to chrome/ and don't involve content/ at all.
  // That suggests they belong somewhere else at the chrome/ layer.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Notification that the application locale has changed. This allows us to
  // update our I/O thread cache of this value.
  static void SetApplicationLocale(const std::string& locale);

  // content::ContentBrowserClient:
  content::BrowserMainParts* CreateBrowserMainParts(
      const content::MainFunctionParams& parameters) override;
  void PostAfterStartupTask(const base::Location& from_here,
                            const scoped_refptr<base::TaskRunner>& task_runner,
                            base::OnceClosure task) override;
  bool IsBrowserStartupComplete() override;
  void SetBrowserStartupIsCompleteForTesting() override;
  std::string GetStoragePartitionIdForSite(
      content::BrowserContext* browser_context,
      const GURL& site) override;
  bool IsValidStoragePartitionId(content::BrowserContext* browser_context,
                                 const std::string& partition_id) override;
  void GetStoragePartitionConfigForSite(
      content::BrowserContext* browser_context,
      const GURL& site,
      bool can_be_default,
      std::string* partition_domain,
      std::string* partition_name,
      bool* in_memory) override;
  content::WebContentsViewDelegate* GetWebContentsViewDelegate(
      content::WebContents* web_contents) override;
  void RenderProcessWillLaunch(
      content::RenderProcessHost* host,
      service_manager::mojom::ServiceRequest* service_request) override;
  bool AllowGpuLaunchRetryOnIOThread() override;
  GURL GetEffectiveURL(content::BrowserContext* browser_context,
                       const GURL& url) override;
  bool ShouldCompareEffectiveURLsForSiteInstanceSelection(
      content::BrowserContext* browser_context,
      content::SiteInstance* candidate_site_instance,
      bool is_main_frame,
      const GURL& candidate_url,
      const GURL& destination_url) override;
  bool ShouldUseMobileFlingCurve() const override;
  bool ShouldUseProcessPerSite(content::BrowserContext* browser_context,
                               const GURL& effective_url) override;
  bool ShouldUseSpareRenderProcessHost(content::BrowserContext* browser_context,
                                       const GURL& site_url) override;
  bool DoesSiteRequireDedicatedProcess(content::BrowserContext* browser_context,
                                       const GURL& effective_site_url) override;
  bool ShouldLockToOrigin(content::BrowserContext* browser_context,
                          const GURL& effective_site_url) override;
  const char* GetInitiatorSchemeBypassingDocumentBlocking() override;
  void LogInitiatorSchemeBypassingDocumentBlocking(
      const url::Origin& initiator_origin,
      int render_process_id,
      content::ResourceType resource_type) override;
  network::mojom::URLLoaderFactoryPtrInfo
  CreateURLLoaderFactoryForNetworkRequests(
      content::RenderProcessHost* process,
      network::mojom::NetworkContext* network_context,
      const url::Origin& request_initiator) override;
  void GetAdditionalWebUISchemes(
      std::vector<std::string>* additional_schemes) override;
  void GetAdditionalViewSourceSchemes(
      std::vector<std::string>* additional_schemes) override;
  bool LogWebUIUrl(const GURL& web_ui_url) const override;
  bool IsWebUIAllowedToMakeNetworkRequests(const url::Origin& origin) override;
  bool IsHandledURL(const GURL& url) override;
  bool CanCommitURL(content::RenderProcessHost* process_host,
                    const GURL& url) override;
  bool ShouldAllowOpenURL(content::SiteInstance* site_instance,
                          const GURL& url) override;
  void OverrideNavigationParams(content::SiteInstance* site_instance,
                                ui::PageTransition* transition,
                                bool* is_renderer_initiated,
                                content::Referrer* referrer) override;
  bool ShouldStayInParentProcessForNTP(
      const GURL& url,
      content::SiteInstance* parent_site_instance) override;
  bool IsSuitableHost(content::RenderProcessHost* process_host,
                      const GURL& site_url) override;
  bool MayReuseHost(content::RenderProcessHost* process_host) override;
  bool ShouldTryToUseExistingProcessHost(
      content::BrowserContext* browser_context,
      const GURL& url) override;
  void SiteInstanceGotProcess(content::SiteInstance* site_instance) override;
  void SiteInstanceDeleting(content::SiteInstance* site_instance) override;
  bool ShouldSwapBrowsingInstancesForNavigation(
      content::SiteInstance* site_instance,
      const GURL& current_url,
      const GURL& new_url) override;
  bool ShouldIsolateErrorPage(bool in_main_frame) override;
  bool ShouldAssignSiteForURL(const GURL& url) override;
  std::vector<url::Origin> GetOriginsRequiringDedicatedProcess() override;
  bool ShouldEnableStrictSiteIsolation() override;
  bool IsFileAccessAllowed(const base::FilePath& path,
                           const base::FilePath& absolute_path,
                           const base::FilePath& profile_path) override;
  void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                      int child_process_id) override;
  void AdjustUtilityServiceProcessCommandLine(
      const service_manager::Identity& identity,
      base::CommandLine* command_line) override;
  std::string GetApplicationLocale() override;
  std::string GetAcceptLangs(content::BrowserContext* context) override;
  const gfx::ImageSkia* GetDefaultFavicon() override;
  bool IsDataSaverEnabled(content::BrowserContext* context) override;
  void UpdateRendererPreferencesForWorker(
      content::BrowserContext* browser_context,
      content::RendererPreferences* out_prefs) override;
  void NavigationRequestStarted(
      int frame_tree_node_id,
      const GURL& url,
      std::unique_ptr<net::HttpRequestHeaders>* extra_headers,
      int* extra_load_flags) override;
  void NavigationRequestRedirected(int frame_tree_node_id,
                                   const GURL& url,
                                   base::Optional<net::HttpRequestHeaders>*
                                       modified_request_headers) override;
  bool AllowAppCache(const GURL& manifest_url,
                     const GURL& first_party,
                     content::ResourceContext* context) override;
  bool AllowServiceWorker(
      const GURL& scope,
      const GURL& first_party,
      content::ResourceContext* context,
      base::RepeatingCallback<content::WebContents*()> wc_getter) override;
  bool AllowSharedWorker(const GURL& worker_url,
                         const GURL& main_frame_url,
                         const std::string& name,
                         const url::Origin& constructor_origin,
                         content::BrowserContext* context,
                         int render_process_id,
                         int render_frame_id) override;
  bool AllowGetCookie(const GURL& url,
                      const GURL& first_party,
                      const net::CookieList& cookie_list,
                      content::ResourceContext* context,
                      int render_process_id,
                      int render_frame_id) override;
  bool AllowSetCookie(const GURL& url,
                      const GURL& first_party,
                      const net::CanonicalCookie& cookie,
                      content::ResourceContext* context,
                      int render_process_id,
                      int render_frame_id) override;
  void OnCookiesRead(int process_id,
                     int routing_id,
                     const GURL& url,
                     const GURL& first_party_url,
                     const net::CookieList& cookie_list,
                     bool blocked_by_policy) override;
  void OnCookieChange(int process_id,
                      int routing_id,
                      const GURL& url,
                      const GURL& first_party_url,
                      const net::CanonicalCookie& cookie,
                      bool blocked_by_policy) override;
  void AllowWorkerFileSystem(
      const GURL& url,
      content::ResourceContext* context,
      const std::vector<content::GlobalFrameRoutingId>& render_frames,
      base::Callback<void(bool)> callback) override;
  bool AllowWorkerIndexedDB(
      const GURL& url,
      const base::string16& name,
      content::ResourceContext* context,
      const std::vector<content::GlobalFrameRoutingId>& render_frames) override;
  AllowWebBluetoothResult AllowWebBluetooth(
      content::BrowserContext* browser_context,
      const url::Origin& requesting_origin,
      const url::Origin& embedding_origin) override;
  std::string GetWebBluetoothBlocklist() override;
#if defined(OS_CHROMEOS)
  void OnUsedTrustAnchor(const std::string& username_hash) override;
#endif
  net::CookieStore* OverrideCookieStoreForURL(
      const GURL& url,
      content::ResourceContext* context) override;
  scoped_refptr<network::SharedURLLoaderFactory>
  GetSystemSharedURLLoaderFactory() override;
  std::string GetGeolocationApiKey() override;

#if defined(OS_ANDROID)
  bool ShouldUseGmsCoreGeolocationProvider() override;
#endif

  content::QuotaPermissionContext* CreateQuotaPermissionContext() override;
  void GetQuotaSettings(
      content::BrowserContext* context,
      content::StoragePartition* partition,
      storage::OptionalQuotaSettingsCallback callback) override;
  content::GeneratedCodeCacheSettings GetGeneratedCodeCacheSettings(
      content::BrowserContext* context) override;
  void AllowCertificateError(
      content::WebContents* web_contents,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      content::ResourceType resource_type,
      bool strict_enforcement,
      bool expired_previous_decision,
      const base::Callback<void(content::CertificateRequestResultType)>&
          callback) override;
  void SelectClientCertificate(
      content::WebContents* web_contents,
      net::SSLCertRequestInfo* cert_request_info,
      net::ClientCertIdentityList client_certs,
      std::unique_ptr<content::ClientCertificateDelegate> delegate) override;
  content::MediaObserver* GetMediaObserver() override;
  content::PlatformNotificationService* GetPlatformNotificationService()
      override;
  bool CanCreateWindow(content::RenderFrameHost* opener,
                       const GURL& opener_url,
                       const GURL& opener_top_level_frame_url,
                       const GURL& source_origin,
                       content::mojom::WindowContainerType container_type,
                       const GURL& target_url,
                       const content::Referrer& referrer,
                       const std::string& frame_name,
                       WindowOpenDisposition disposition,
                       const blink::mojom::WindowFeatures& features,
                       bool user_gesture,
                       bool opener_suppressed,
                       bool* no_javascript_access) override;
  void ResourceDispatcherHostCreated() override;
  content::SpeechRecognitionManagerDelegate*
  CreateSpeechRecognitionManagerDelegate() override;
  net::NetLog* GetNetLog() override;
  void OverrideWebkitPrefs(content::RenderViewHost* rvh,
                           content::WebPreferences* prefs) override;
  void BrowserURLHandlerCreated(content::BrowserURLHandler* handler) override;
  base::FilePath GetDefaultDownloadDirectory() override;
  std::string GetDefaultDownloadName() override;
  base::FilePath GetShaderDiskCacheDirectory() override;
  base::FilePath GetGrShaderDiskCacheDirectory() override;
  void DidCreatePpapiPlugin(content::BrowserPpapiHost* browser_host) override;
  content::BrowserPpapiHost* GetExternalBrowserPpapiHost(
      int plugin_process_id) override;
  bool AllowPepperSocketAPI(
      content::BrowserContext* browser_context,
      const GURL& url,
      bool private_api,
      const content::SocketPermissionRequest* params) override;
  bool IsPepperVpnProviderAPIAllowed(content::BrowserContext* browser_context,
                                     const GURL& url) override;
  std::unique_ptr<content::VpnServiceProxy> GetVpnServiceProxy(
      content::BrowserContext* browser_context) override;
  std::unique_ptr<ui::SelectFilePolicy> CreateSelectFilePolicy(
      content::WebContents* web_contents) override;
  void GetAdditionalAllowedSchemesForFileSystem(
      std::vector<std::string>* additional_schemes) override;
  void GetSchemesBypassingSecureContextCheckWhitelist(
      std::set<std::string>* schemes) override;
  void GetURLRequestAutoMountHandlers(
      std::vector<storage::URLRequestAutoMountHandler>* handlers) override;
  void GetAdditionalFileSystemBackends(
      content::BrowserContext* browser_context,
      const base::FilePath& storage_partition_path,
      std::vector<std::unique_ptr<storage::FileSystemBackend>>*
          additional_backends) override;
  content::DevToolsManagerDelegate* GetDevToolsManagerDelegate() override;
  content::TracingDelegate* GetTracingDelegate() override;
  bool IsPluginAllowedToCallRequestOSFileHandle(
      content::BrowserContext* browser_context,
      const GURL& url) override;
  bool IsPluginAllowedToUseDevChannelAPIs(
      content::BrowserContext* browser_context,
      const GURL& url) override;
  void OverridePageVisibilityState(
      content::RenderFrameHost* render_frame_host,
      blink::mojom::PageVisibilityState* visibility_state) override;
#if defined(OS_POSIX) && !defined(OS_MACOSX)
  void GetAdditionalMappedFilesForChildProcess(
      const base::CommandLine& command_line,
      int child_process_id,
      content::PosixFileDescriptorInfo* mappings) override;
#endif  // defined(OS_POSIX) && !defined(OS_MACOSX)
#if defined(OS_WIN)
  bool PreSpawnRenderer(sandbox::TargetPolicy* policy) override;
  base::string16 GetAppContainerSidForSandboxType(
      int sandbox_type) const override;
#endif
  void ExposeInterfacesToRenderer(
      service_manager::BinderRegistry* registry,
      blink::AssociatedInterfaceRegistry* associated_registry,
      content::RenderProcessHost* render_process_host) override;
  void ExposeInterfacesToMediaService(
      service_manager::BinderRegistry* registry,
      content::RenderFrameHost* render_frame_host) override;
  void BindInterfaceRequestFromFrame(
      content::RenderFrameHost* render_frame_host,
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle interface_pipe) override;
  bool BindAssociatedInterfaceRequestFromFrame(
      content::RenderFrameHost* render_frame_host,
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle* handle) override;
  void BindInterfaceRequestFromWorker(
      content::RenderProcessHost* render_process_host,
      const url::Origin& origin,
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle interface_pipe) override;
  void BindInterfaceRequest(
      const service_manager::BindSourceInfo& source_info,
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle* interface_pipe) override;
  void RegisterInProcessServices(
      StaticServiceMap* services,
      content::ServiceManagerConnection* connection) override;
  void RegisterOutOfProcessServices(
      OutOfProcessServiceMap* services) override;
  bool ShouldTerminateOnServiceQuit(
      const service_manager::Identity& id) override;
  std::unique_ptr<base::Value> GetServiceManifestOverlay(
      base::StringPiece name) override;
  std::vector<content::ContentBrowserClient::ServiceManifestInfo>
  GetExtraServiceManifests() override;
  std::vector<service_manager::Identity> GetStartupServices() override;
  void OpenURL(content::BrowserContext* browser_context,
               const content::OpenURLParams& params,
               const base::Callback<void(content::WebContents*)>& callback)
      override;
  content::ControllerPresentationServiceDelegate*
  GetControllerPresentationServiceDelegate(
      content::WebContents* web_contents) override;
  content::ReceiverPresentationServiceDelegate*
  GetReceiverPresentationServiceDelegate(
      content::WebContents* web_contents) override;
  void RecordURLMetric(const std::string& metric, const GURL& url) override;
  std::string GetMetricSuffixForURL(const GURL& url) override;
  std::vector<std::unique_ptr<content::NavigationThrottle>>
  CreateThrottlesForNavigation(content::NavigationHandle* handle) override;
  std::unique_ptr<content::NavigationUIData> GetNavigationUIData(
      content::NavigationHandle* navigation_handle) override;
  void GetHardwareSecureDecryptionCaps(
      const std::string& key_system,
      const base::flat_set<media::CdmProxy::Protocol>& cdm_proxy_protocols,
      base::flat_set<media::VideoCodec>* video_codecs,
      base::flat_set<media::EncryptionMode>* encryption_schemes) override;
  ::rappor::RapporService* GetRapporService() override;
#if BUILDFLAG(ENABLE_MEDIA_REMOTING)
  void CreateMediaRemoter(content::RenderFrameHost* render_frame_host,
                          media::mojom::RemotingSourcePtr source,
                          media::mojom::RemoterRequest request) final;
#endif  // BUILDFLAG(ENABLE_MEDIA_REMOTING)
  std::unique_ptr<base::TaskScheduler::InitParams> GetTaskSchedulerInitParams()
      override;
  base::FilePath GetLoggingFileName(
      const base::CommandLine& command_line) override;
  std::vector<std::unique_ptr<content::URLLoaderThrottle>>
  CreateURLLoaderThrottles(
      const network::ResourceRequest& request,
      content::ResourceContext* resource_context,
      const base::RepeatingCallback<content::WebContents*()>& wc_getter,
      content::NavigationUIData* navigation_ui_data,
      int frame_tree_node_id) override;
  void RegisterNonNetworkNavigationURLLoaderFactories(
      int frame_tree_node_id,
      NonNetworkURLLoaderFactoryMap* factories) override;
  void RegisterNonNetworkSubresourceURLLoaderFactories(
      int render_process_id,
      int render_frame_id,
      NonNetworkURLLoaderFactoryMap* factories) override;
  bool WillCreateURLLoaderFactory(
      content::BrowserContext* browser_context,
      content::RenderFrameHost* frame,
      bool is_navigation,
      const url::Origin& request_initiator,
      network::mojom::URLLoaderFactoryRequest* factory_request,
      bool* bypass_redirect_checks) override;
  std::vector<std::unique_ptr<content::URLLoaderRequestInterceptor>>
  WillCreateURLLoaderRequestInterceptors(
      content::NavigationUIData* navigation_ui_data,
      int frame_tree_node_id) override;
  void WillCreateWebSocket(
      content::RenderFrameHost* frame,
      network::mojom::WebSocketRequest* request,
      network::mojom::AuthenticationHandlerPtr* auth_handler) override;
  void OnNetworkServiceCreated(
      network::mojom::NetworkService* network_service) override;
  network::mojom::NetworkContextPtr CreateNetworkContext(
      content::BrowserContext* context,
      bool in_memory,
      const base::FilePath& relative_partition_path) override;
  bool AllowRenderingMhtmlOverHttp(
      content::NavigationUIData* navigation_ui_data) override;
  bool ShouldForceDownloadResource(const GURL& url,
                                   const std::string& mime_type) override;
  void CreateWebUsbService(
      content::RenderFrameHost* render_frame_host,
      mojo::InterfaceRequest<blink::mojom::WebUsbService> request) override;
  bool ShowPaymentHandlerWindow(
      content::BrowserContext* browser_context,
      const GURL& url,
      base::OnceCallback<void(bool, int, int)> callback) override;
  std::unique_ptr<content::AuthenticatorRequestClientDelegate>
  GetWebAuthenticationRequestDelegate(
      content::RenderFrameHost* render_frame_host) override;
#if defined(OS_MACOSX)
  bool IsWebAuthenticationTouchIdAuthenticatorSupported() override;
#endif
  std::unique_ptr<net::ClientCertStore> CreateClientCertStore(
      content::ResourceContext* resource_context) override;
  scoped_refptr<content::LoginDelegate> CreateLoginDelegate(
      net::AuthChallengeInfo* auth_info,
      content::ResourceRequestInfo::WebContentsGetter web_contents_getter,
      const content::GlobalRequestID& request_id,
      bool is_request_for_main_frame,
      const GURL& url,
      scoped_refptr<net::HttpResponseHeaders> response_headers,
      bool first_auth_attempt,
      LoginAuthRequiredCallback auth_required_callback) override;
  bool HandleExternalProtocol(
      const GURL& url,
      content::ResourceRequestInfo::WebContentsGetter web_contents_getter,
      int child_id,
      content::NavigationUIData* navigation_data,
      bool is_main_frame,
      ui::PageTransition page_transition,
      bool has_user_gesture) override;
  std::unique_ptr<content::OverlayWindow> CreateWindowForPictureInPicture(
      content::PictureInPictureWindowController* controller) override;
  bool IsSafeRedirectTarget(const GURL& url,
                            content::ResourceContext* context) override;
  void RegisterRendererPreferenceWatcherForWorkers(
      content::BrowserContext* browser_context,
      content::mojom::RendererPreferenceWatcherPtr watcher) override;
  base::Optional<std::string> GetOriginPolicyErrorPage(
      content::OriginPolicyErrorReason error_reason,
      const url::Origin& origin,
      const GURL& url) override;
  bool CanIgnoreCertificateErrorIfNeeded() override;
  void OnNetworkServiceDataUseUpdate(int32_t network_traffic_annotation_id_hash,
                                     int64_t recv_bytes,
                                     int64_t sent_bytes) override;

  content::PreviewsState DetermineAllowedPreviews(
      content::PreviewsState initial_state,
      content::NavigationHandle* navigation_handle) override;

  content::PreviewsState DetermineCommittedPreviews(
      content::PreviewsState initial_state,
      content::NavigationHandle* navigation_handle,
      const net::HttpResponseHeaders* response_headers) override;

  // Determines the committed previews state for the passed in params.
  static content::PreviewsState DetermineCommittedPreviewsForURL(
      const GURL& url,
      data_reduction_proxy::DataReductionProxyData* drp_data,
      previews::PreviewsUserData* previews_user_data,
      const previews::PreviewsDecider* previews_decider,
      content::PreviewsState initial_state);

 protected:
  static bool HandleWebUI(GURL* url, content::BrowserContext* browser_context);
  static bool HandleWebUIReverse(GURL* url,
                                 content::BrowserContext* browser_context);

 private:
  friend class DisableWebRtcEncryptionFlagTest;
  friend class InProcessBrowserTest;

  // Populate |frame_interfaces_|, |frame_interfaces_parameterized_| and
  // |worker_interfaces_parameterized_|.
  void InitWebContextInterfaces();

  // Copies disable WebRTC encryption switch depending on the channel.
  static void MaybeCopyDisableWebRtcEncryptionSwitch(
      base::CommandLine* to_command_line,
      const base::CommandLine& from_command_line,
      version_info::Channel channel);

  void FileSystemAccessed(
      const GURL& url,
      const std::vector<content::GlobalFrameRoutingId>& render_frames,
      base::Callback<void(bool)> callback,
      bool allow);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  void GuestPermissionRequestHelper(
      const GURL& url,
      const std::vector<content::GlobalFrameRoutingId>& render_frames,
      base::Callback<void(bool)> callback,
      bool allow);

  static void RequestFileSystemPermissionOnUIThread(
      int render_process_id,
      int render_frame_id,
      const GURL& url,
      bool allowed_by_default,
      const base::Callback<void(bool)>& callback);
#endif

  // The value pointed to by |settings| should remain valid until the
  // the function is called again with a new value or a nullptr.
  static void SetDefaultQuotaSettingsForTesting(
      const storage::QuotaSettings *settings);

  safe_browsing::UrlCheckerDelegate* GetSafeBrowsingUrlCheckerDelegate(
      content::ResourceContext* resource_context);

#if BUILDFLAG(ENABLE_PLUGINS)
  // Set of origins that can use TCP/UDP private APIs from NaCl.
  std::set<std::string> allowed_socket_origins_;
  // Set of origins that can get a handle for FileIO from NaCl.
  std::set<std::string> allowed_file_handle_origins_;
  // Set of origins that can use "dev chanel" APIs from NaCl, even on stable
  // versions of Chrome.
  std::set<std::string> allowed_dev_channel_origins_;
#endif

  // Vector of additional ChromeContentBrowserClientParts.
  // Parts are deleted in the reverse order they are added.
  std::vector<ChromeContentBrowserClientParts*> extra_parts_;

  service_manager::BinderRegistry gpu_binder_registry_;

  scoped_refptr<safe_browsing::SafeBrowsingService> safe_browsing_service_;
  scoped_refptr<safe_browsing::UrlCheckerDelegate>
      safe_browsing_url_checker_delegate_;

  std::unique_ptr<service_manager::BinderRegistry> frame_interfaces_;
  std::unique_ptr<
      service_manager::BinderRegistryWithArgs<content::RenderFrameHost*>>
      frame_interfaces_parameterized_;
  std::unique_ptr<
      service_manager::BinderRegistryWithArgs<content::RenderProcessHost*,
                                              const url::Origin&>>
      worker_interfaces_parameterized_;

  ChromeFeatureListCreator* chrome_feature_list_creator_;

  base::WeakPtrFactory<ChromeContentBrowserClient> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChromeContentBrowserClient);
};

#endif  // CHROME_BROWSER_CHROME_CONTENT_BROWSER_CLIENT_H_
