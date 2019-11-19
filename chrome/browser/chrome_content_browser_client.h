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
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/startup_data.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/previews_state.h"
#include "content/public/common/resource_type.h"
#include "extensions/buildflags/buildflags.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "storage/browser/quota/quota_settings.h"

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
class URLLoaderThrottle;
}

namespace content {
class BrowserContext;
class QuotaPermissionContext;
}

namespace data_reduction_proxy {
class DataReductionProxyData;
class DataReductionProxyThrottleManager;
}  // namespace data_reduction_proxy

namespace previews {
class PreviewsDecider;
class PreviewsUserData;
}  // namespace previews

namespace safe_browsing {
class SafeBrowsingService;
class UrlCheckerDelegate;
}

namespace ui {
class NativeTheme;
}

namespace url {
class Origin;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace version_info {
enum class Channel;
}

class ChromeHidDelegate;
class ChromeSerialDelegate;

// Returns the user agent of Chrome.
std::string GetUserAgent();

blink::UserAgentMetadata GetUserAgentMetadata();

class ChromeContentBrowserClient : public content::ContentBrowserClient {
 public:
  explicit ChromeContentBrowserClient(StartupData* startup_data = nullptr);
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
  std::unique_ptr<content::BrowserMainParts> CreateBrowserMainParts(
      const content::MainFunctionParams& parameters) override;
  void PostAfterStartupTask(const base::Location& from_here,
                            const scoped_refptr<base::TaskRunner>& task_runner,
                            base::OnceClosure task) override;
  bool IsBrowserStartupComplete() override;
  void SetBrowserStartupIsCompleteForTesting() override;
  std::string GetStoragePartitionIdForSite(
      content::BrowserContext* browser_context,
      const GURL& site) override;
  bool IsShuttingDown() override;
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
  void RenderProcessWillLaunch(content::RenderProcessHost* host) override;
  bool AllowGpuLaunchRetryOnIOThread() override;
  GURL GetEffectiveURL(content::BrowserContext* browser_context,
                       const GURL& url) override;
  bool ShouldCompareEffectiveURLsForSiteInstanceSelection(
      content::BrowserContext* browser_context,
      content::SiteInstance* candidate_site_instance,
      bool is_main_frame,
      const GURL& candidate_url,
      const GURL& destination_url) override;
  bool ShouldUseMobileFlingCurve() override;
  bool ShouldUseProcessPerSite(content::BrowserContext* browser_context,
                               const GURL& effective_url) override;
  bool ShouldUseSpareRenderProcessHost(content::BrowserContext* browser_context,
                                       const GURL& site_url) override;
  bool DoesSiteRequireDedicatedProcess(content::BrowserContext* browser_context,
                                       const GURL& effective_site_url) override;
  bool ShouldLockToOrigin(content::BrowserContext* browser_context,
                          const GURL& effective_site_url) override;
  const char* GetInitiatorSchemeBypassingDocumentBlocking() override;
  bool ShouldTreatURLSchemeAsFirstPartyWhenTopLevel(
      base::StringPiece scheme,
      bool is_embedded_origin_secure) override;
  bool ShouldIgnoreSameSiteCookieRestrictionsWhenTopLevel(
      base::StringPiece scheme,
      bool is_embedded_origin_secure) override;
  mojo::PendingRemote<network::mojom::URLLoaderFactory>
  CreateURLLoaderFactoryForNetworkRequests(
      content::RenderProcessHost* process,
      network::mojom::NetworkContext* network_context,
      mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
          header_client,
      const url::Origin& origin,
      const url::Origin& main_world_origin,
      const base::Optional<net::NetworkIsolationKey>& network_isolation_key)
      override;
  void GetAdditionalWebUISchemes(
      std::vector<std::string>* additional_schemes) override;
  void GetAdditionalViewSourceSchemes(
      std::vector<std::string>* additional_schemes) override;
  bool LogWebUIUrl(const GURL& web_ui_url) override;
  bool IsWebUIAllowedToMakeNetworkRequests(const url::Origin& origin) override;
  bool IsHandledURL(const GURL& url) override;
  bool CanCommitURL(content::RenderProcessHost* process_host,
                    const GURL& url) override;
  void OverrideNavigationParams(
      content::SiteInstance* site_instance,
      ui::PageTransition* transition,
      bool* is_renderer_initiated,
      content::Referrer* referrer,
      base::Optional<url::Origin>* initiator_origin) override;
  bool ShouldStayInParentProcessForNTP(
      const GURL& url,
      content::SiteInstance* parent_site_instance) override;
  bool IsSuitableHost(content::RenderProcessHost* process_host,
                      const GURL& site_url) override;
  bool MayReuseHost(content::RenderProcessHost* process_host) override;
  bool ShouldTryToUseExistingProcessHost(
      content::BrowserContext* browser_context,
      const GURL& url) override;
  bool ShouldSubframesTryToReuseExistingProcess(
      content::RenderFrameHost* main_frame) override;
  void SiteInstanceGotProcess(content::SiteInstance* site_instance) override;
  void SiteInstanceDeleting(content::SiteInstance* site_instance) override;
  bool ShouldSwapBrowsingInstancesForNavigation(
      content::SiteInstance* site_instance,
      const GURL& current_effective_url,
      const GURL& destination_effective_url) override;
  bool ShouldIsolateErrorPage(bool in_main_frame) override;
  bool ShouldAssignSiteForURL(const GURL& url) override;
  std::vector<url::Origin> GetOriginsRequiringDedicatedProcess() override;
  bool ShouldEnableStrictSiteIsolation() override;
  bool ShouldDisableSiteIsolation() override;
  std::vector<std::string> GetAdditionalSiteIsolationModes() override;
  void PersistIsolatedOrigin(content::BrowserContext* context,
                             const url::Origin& origin) override;
  bool IsFileAccessAllowed(const base::FilePath& path,
                           const base::FilePath& absolute_path,
                           const base::FilePath& profile_path) override;
  void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                      int child_process_id) override;
  void AdjustUtilityServiceProcessCommandLine(
      const service_manager::Identity& identity,
      base::CommandLine* command_line) override;
  std::string GetApplicationClientGUIDForQuarantineCheck() override;
  std::string GetApplicationLocale() override;
  std::string GetAcceptLangs(content::BrowserContext* context) override;
  gfx::ImageSkia GetDefaultFavicon() override;
  bool IsDataSaverEnabled(content::BrowserContext* context) override;
  void UpdateRendererPreferencesForWorker(
      content::BrowserContext* browser_context,
      blink::mojom::RendererPreferences* out_prefs) override;
  bool AllowAppCache(const GURL& manifest_url,
                     const GURL& first_party,
                     content::BrowserContext* context) override;
  bool AllowServiceWorkerOnIO(
      const GURL& scope,
      const GURL& site_for_cookies,
      const base::Optional<url::Origin>& top_frame_origin,
      const GURL& script_url,
      content::ResourceContext* context,
      base::RepeatingCallback<content::WebContents*()> wc_getter) override;
  bool AllowServiceWorkerOnUI(
      const GURL& scope,
      const GURL& site_for_cookies,
      const base::Optional<url::Origin>& top_frame_origin,
      const GURL& script_url,
      content::BrowserContext* context,
      base::RepeatingCallback<content::WebContents*()> wc_getter) override;
  bool AllowSharedWorker(const GURL& worker_url,
                         const GURL& site_for_cookies,
                         const base::Optional<url::Origin>& top_frame_origin,
                         const std::string& name,
                         const url::Origin& constructor_origin,
                         content::BrowserContext* context,
                         int render_process_id,
                         int render_frame_id) override;
  bool DoesSchemeAllowCrossOriginSharedWorker(
      const std::string& scheme) override;
  bool AllowSignedExchange(content::BrowserContext* browser_context) override;
  void AllowWorkerFileSystem(
      const GURL& url,
      content::BrowserContext* browser_context,
      const std::vector<content::GlobalFrameRoutingId>& render_frames,
      base::OnceCallback<void(bool)> callback) override;
  bool AllowWorkerIndexedDB(
      const GURL& url,
      content::BrowserContext* browser_context,
      const std::vector<content::GlobalFrameRoutingId>& render_frames) override;
  bool AllowWorkerCacheStorage(
      const GURL& url,
      content::BrowserContext* browser_context,
      const std::vector<content::GlobalFrameRoutingId>& render_frames) override;
  bool AllowWorkerWebLocks(
      const GURL& url,
      content::BrowserContext* browser_context,
      const std::vector<content::GlobalFrameRoutingId>& render_frames) override;
  AllowWebBluetoothResult AllowWebBluetooth(
      content::BrowserContext* browser_context,
      const url::Origin& requesting_origin,
      const url::Origin& embedding_origin) override;
  std::string GetWebBluetoothBlocklist() override;
#if defined(OS_CHROMEOS)
  void OnTrustAnchorUsed(content::BrowserContext* browser_context) override;
#endif
  scoped_refptr<network::SharedURLLoaderFactory>
  GetSystemSharedURLLoaderFactory() override;
  network::mojom::NetworkContext* GetSystemNetworkContext() override;
  std::string GetGeolocationApiKey() override;

#if defined(OS_ANDROID)
  bool ShouldUseGmsCoreGeolocationProvider() override;
#endif
  scoped_refptr<content::QuotaPermissionContext> CreateQuotaPermissionContext()
      override;
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
      bool is_main_frame_request,
      bool strict_enforcement,
      const base::Callback<void(content::CertificateRequestResultType)>&
          callback) override;
  base::OnceClosure SelectClientCertificate(
      content::WebContents* web_contents,
      net::SSLCertRequestInfo* cert_request_info,
      net::ClientCertIdentityList client_certs,
      std::unique_ptr<content::ClientCertificateDelegate> delegate) override;
  content::MediaObserver* GetMediaObserver() override;
  content::LockObserver* GetLockObserver() override;
  content::PlatformNotificationService* GetPlatformNotificationService(
      content::BrowserContext* browser_context) override;
  bool CanCreateWindow(content::RenderFrameHost* opener,
                       const GURL& opener_url,
                       const GURL& opener_top_level_frame_url,
                       const url::Origin& source_origin,
                       content::mojom::WindowContainerType container_type,
                       const GURL& target_url,
                       const content::Referrer& referrer,
                       const std::string& frame_name,
                       WindowOpenDisposition disposition,
                       const blink::mojom::WindowFeatures& features,
                       bool user_gesture,
                       bool opener_suppressed,
                       bool* no_javascript_access) override;
  content::SpeechRecognitionManagerDelegate*
  CreateSpeechRecognitionManagerDelegate() override;
  content::TtsControllerDelegate* GetTtsControllerDelegate() override;
  content::TtsPlatform* GetTtsPlatform() override;
  void OverrideWebkitPrefs(content::RenderViewHost* rvh,
                           content::WebPreferences* prefs) override;
  void BrowserURLHandlerCreated(content::BrowserURLHandler* handler) override;
  base::FilePath GetDefaultDownloadDirectory() override;
  std::string GetDefaultDownloadName() override;
  base::FilePath GetFontLookupTableCacheDir() override;
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
  void UpdateDevToolsBackgroundServiceExpiration(
      content::BrowserContext* browser_context,
      int service,
      base::Time expiration_time) override;
  base::flat_map<int, base::Time> GetDevToolsBackgroundServiceExpirations(
      content::BrowserContext* browser_context) override;
  content::TracingDelegate* GetTracingDelegate() override;
  bool IsPluginAllowedToCallRequestOSFileHandle(
      content::BrowserContext* browser_context,
      const GURL& url) override;
  bool IsPluginAllowedToUseDevChannelAPIs(
      content::BrowserContext* browser_context,
      const GURL& url) override;
  void OverridePageVisibilityState(
      content::RenderFrameHost* render_frame_host,
      content::PageVisibilityState* visibility_state) override;
#if defined(OS_POSIX) && !defined(OS_MACOSX)
  void GetAdditionalMappedFilesForChildProcess(
      const base::CommandLine& command_line,
      int child_process_id,
      content::PosixFileDescriptorInfo* mappings) override;
#endif  // defined(OS_POSIX) && !defined(OS_MACOSX)
#if defined(OS_WIN)
  bool PreSpawnRenderer(sandbox::TargetPolicy* policy,
                        RendererSpawnFlags flags) override;
  base::string16 GetAppContainerSidForSandboxType(int sandbox_type) override;
  bool IsRendererCodeIntegrityEnabled() override;
#endif
  void ExposeInterfacesToRenderer(
      service_manager::BinderRegistry* registry,
      blink::AssociatedInterfaceRegistry* associated_registry,
      content::RenderProcessHost* render_process_host) override;
  void ExposeInterfacesToMediaService(
      service_manager::BinderRegistry* registry,
      content::RenderFrameHost* render_frame_host) override;
  void RegisterBrowserInterfaceBindersForFrame(
      service_manager::BinderMapWithContext<content::RenderFrameHost*>* map)
      override;
  void BindInterfaceRequestFromFrame(
      content::RenderFrameHost* render_frame_host,
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle interface_pipe) override;
  void BindCredentialManagerReceiver(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::CredentialManager> receiver) override;
  bool BindAssociatedReceiverFromFrame(
      content::RenderFrameHost* render_frame_host,
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle* handle) override;
  void BindInterfaceRequestFromWorker(
      content::RenderProcessHost* render_process_host,
      const url::Origin& origin,
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle interface_pipe) override;
  void BindGpuHostReceiver(mojo::GenericPendingReceiver receiver) override;
  void BindHostReceiverForRenderer(
      content::RenderProcessHost* render_process_host,
      mojo::GenericPendingReceiver receiver) override;
  void BindHostReceiverForRendererOnIOThread(
      int render_process_id,
      mojo::GenericPendingReceiver* receiver) override;
  void WillStartServiceManager() override;
  void RunServiceInstance(
      const service_manager::Identity& identity,
      mojo::PendingReceiver<service_manager::mojom::Service>* receiver)
      override;
  base::Optional<service_manager::Manifest> GetServiceManifestOverlay(
      base::StringPiece name) override;
  std::vector<service_manager::Manifest> GetExtraServiceManifests() override;
  void OpenURL(
      content::SiteInstance* site_instance,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::WebContents*)> callback) override;
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
      base::flat_set<media::EncryptionScheme>* encryption_schemes) override;
  ::rappor::RapporService* GetRapporService() override;
#if BUILDFLAG(ENABLE_MEDIA_REMOTING)
  void CreateMediaRemoter(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingRemote<media::mojom::RemotingSource> source,
      mojo::PendingReceiver<media::mojom::Remoter> receiver) final;
#endif  // BUILDFLAG(ENABLE_MEDIA_REMOTING)
  base::FilePath GetLoggingFileName(
      const base::CommandLine& command_line) override;
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
  CreateURLLoaderThrottles(
      const network::ResourceRequest& request,
      content::BrowserContext* browser_context,
      const base::RepeatingCallback<content::WebContents*()>& wc_getter,
      content::NavigationUIData* navigation_ui_data,
      int frame_tree_node_id) override;
  void RegisterNonNetworkNavigationURLLoaderFactories(
      int frame_tree_node_id,
      NonNetworkURLLoaderFactoryMap* factories) override;
  void RegisterNonNetworkWorkerMainResourceURLLoaderFactories(
      content::BrowserContext* browser_context,
      NonNetworkURLLoaderFactoryMap* factories) override;
  void RegisterNonNetworkServiceWorkerUpdateURLLoaderFactories(
      content::BrowserContext* browser_context,
      NonNetworkURLLoaderFactoryMap* factories) override;
  void RegisterNonNetworkSubresourceURLLoaderFactories(
      int render_process_id,
      int render_frame_id,
      NonNetworkURLLoaderFactoryMap* factories) override;
  bool WillCreateURLLoaderFactory(
      content::BrowserContext* browser_context,
      content::RenderFrameHost* frame,
      int render_process_id,
      URLLoaderFactoryType type,
      const url::Origin& request_initiator,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>* factory_receiver,
      mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
          header_client,
      bool* bypass_redirect_checks) override;
  std::vector<std::unique_ptr<content::URLLoaderRequestInterceptor>>
  WillCreateURLLoaderRequestInterceptors(
      content::NavigationUIData* navigation_ui_data,
      int frame_tree_node_id,
      const scoped_refptr<network::SharedURLLoaderFactory>&
          network_loader_factory) override;
  bool WillInterceptWebSocket(content::RenderFrameHost* frame) override;
  void CreateWebSocket(
      content::RenderFrameHost* frame,
      WebSocketFactory factory,
      const GURL& url,
      const GURL& site_for_cookies,
      const base::Optional<std::string>& user_agent,
      mojo::PendingRemote<network::mojom::WebSocketHandshakeClient>
          handshake_client) override;
  bool WillCreateRestrictedCookieManager(
      network::mojom::RestrictedCookieManagerRole role,
      content::BrowserContext* browser_context,
      const url::Origin& origin,
      const GURL& site_for_cookies,
      const url::Origin& top_frame_origin,
      bool is_service_worker,
      int process_id,
      int routing_id,
      mojo::PendingReceiver<network::mojom::RestrictedCookieManager>* receiver)
      override;
  void OnNetworkServiceCreated(
      network::mojom::NetworkService* network_service) override;
  mojo::Remote<network::mojom::NetworkContext> CreateNetworkContext(
      content::BrowserContext* context,
      bool in_memory,
      const base::FilePath& relative_partition_path) override;
  std::vector<base::FilePath> GetNetworkContextsParentDirectory() override;
  bool AllowRenderingMhtmlOverHttp(
      content::NavigationUIData* navigation_ui_data) override;
  bool ShouldForceDownloadResource(const GURL& url,
                                   const std::string& mime_type) override;
  void CreateWebUsbService(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::WebUsbService> receiver) override;
#if !defined(OS_ANDROID)
  content::SerialDelegate* GetSerialDelegate() override;
  content::HidDelegate* GetHidDelegate() override;
  std::unique_ptr<content::AuthenticatorRequestClientDelegate>
  GetWebAuthenticationRequestDelegate(
      content::RenderFrameHost* render_frame_host,
      const std::string& relying_party_id) override;
#endif
  bool ShowPaymentHandlerWindow(
      content::BrowserContext* browser_context,
      const GURL& url,
      base::OnceCallback<void(bool, int, int)> callback) override;
  std::unique_ptr<net::ClientCertStore> CreateClientCertStore(
      content::BrowserContext* browser_context) override;
  std::unique_ptr<content::LoginDelegate> CreateLoginDelegate(
      const net::AuthChallengeInfo& auth_info,
      content::WebContents* web_contents,
      const content::GlobalRequestID& request_id,
      bool is_request_for_main_frame,
      const GURL& url,
      scoped_refptr<net::HttpResponseHeaders> response_headers,
      bool first_auth_attempt,
      LoginAuthRequiredCallback auth_required_callback) override;
  bool HandleExternalProtocol(
      const GURL& url,
      content::WebContents::Getter web_contents_getter,
      int child_id,
      content::NavigationUIData* navigation_data,
      bool is_main_frame,
      ui::PageTransition page_transition,
      bool has_user_gesture,
      const base::Optional<url::Origin>& initiating_origin,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>* out_factory)
      override;
  std::unique_ptr<content::OverlayWindow> CreateWindowForPictureInPicture(
      content::PictureInPictureWindowController* controller) override;
  void RegisterRendererPreferenceWatcher(
      content::BrowserContext* browser_context,
      mojo::PendingRemote<blink::mojom::RendererPreferenceWatcher> watcher)
      override;
  base::Optional<std::string> GetOriginPolicyErrorPage(
      network::OriginPolicyState error_reason,
      content::NavigationHandle* handle) override;
  bool CanAcceptUntrustedExchangesIfNeeded() override;
  void OnNetworkServiceDataUseUpdate(int32_t network_traffic_annotation_id_hash,
                                     int64_t recv_bytes,
                                     int64_t sent_bytes) override;

  content::PreviewsState DetermineAllowedPreviews(
      content::PreviewsState initial_state,
      content::NavigationHandle* navigation_handle,
      const GURL& current_navigation_url) override;

  content::PreviewsState DetermineCommittedPreviews(
      content::PreviewsState initial_state,
      content::NavigationHandle* navigation_handle,
      const net::HttpResponseHeaders* response_headers) override;

  void LogWebFeatureForCurrentPage(content::RenderFrameHost* render_frame_host,
                                   blink::mojom::WebFeature feature) override;

  std::string GetProduct() override;
  std::string GetUserAgent() override;
  blink::UserAgentMetadata GetUserAgentMetadata() override;

  base::Optional<gfx::ImageSkia> GetProductLogo() override;

  bool IsBuiltinComponent(content::BrowserContext* browser_context,
                          const url::Origin& origin) override;

  bool IsRendererDebugURLBlacklisted(const GURL& url,
                                     content::BrowserContext* context) override;

  ui::AXMode GetAXModeForBrowserContext(
      content::BrowserContext* browser_context) override;

#if defined(OS_ANDROID)
  ContentBrowserClient::WideColorGamutHeuristic GetWideColorGamutHeuristic()
      override;
#endif

  base::flat_set<std::string> GetPluginMimeTypesWithExternalHandlers(
      content::BrowserContext* browser_context) override;

  void AugmentNavigationDownloadPolicy(
      const content::WebContents* web_contents,
      const content::RenderFrameHost* frame_host,
      bool user_gesture,
      content::NavigationDownloadPolicy* download_policy) override;

  bool IsBluetoothScanningBlocked(content::BrowserContext* browser_context,
                                  const url::Origin& requesting_origin,
                                  const url::Origin& embedding_origin) override;

  void BlockBluetoothScanning(content::BrowserContext* browser_context,
                              const url::Origin& requesting_origin,
                              const url::Origin& embedding_origin) override;

  bool ShouldLoadExtraIcuDataFile() override;

  bool ArePersistentMediaDeviceIDsAllowed(
      content::BrowserContext* browser_context,
      const GURL& scope,
      const GURL& site_for_cookies,
      const base::Optional<url::Origin>& top_frame_origin) override;

  content::PreviewsState DetermineAllowedPreviewsWithoutHoldback(
      content::PreviewsState initial_state,
      content::NavigationHandle* navigation_handle,
      const GURL& current_navigation_url);

  content::PreviewsState DetermineCommittedPreviewsWithoutHoldback(
      content::PreviewsState initial_state,
      content::NavigationHandle* navigation_handle,
      const net::HttpResponseHeaders* response_headers);

  // Determines the committed previews state for the passed in params.
  static content::PreviewsState DetermineCommittedPreviewsForURL(
      const GURL& url,
      data_reduction_proxy::DataReductionProxyData* drp_data,
      previews::PreviewsUserData* previews_user_data,
      const previews::PreviewsDecider* previews_decider,
      content::PreviewsState initial_state,
      content::NavigationHandle* navigation_handle);

#if !defined(OS_ANDROID)
  void FetchRemoteSms(
      content::BrowserContext* browser_context,
      const url::Origin& origin,
      base::OnceCallback<void(base::Optional<std::string>)> callback) override;
#endif

 protected:
  static bool HandleWebUI(GURL* url, content::BrowserContext* browser_context);
  static bool HandleWebUIReverse(GURL* url,
                                 content::BrowserContext* browser_context);
  virtual ui::NativeTheme* GetWebTheme() const;  // For testing.

 private:
  friend class DisableWebRtcEncryptionFlagTest;
  friend class InProcessBrowserTest;

  // Populate |frame_interfaces_|, |frame_interfaces_parameterized_| and
  // |worker_interfaces_parameterized_|.
  void InitWebContextInterfaces();

  // Initializes |network_contexts_parent_directory_| on the UI thread.
  void InitNetworkContextsParentDirectory();

  // Copies disable WebRTC encryption switch depending on the channel.
  static void MaybeCopyDisableWebRtcEncryptionSwitch(
      base::CommandLine* to_command_line,
      const base::CommandLine& from_command_line,
      version_info::Channel channel);

  void FileSystemAccessed(
      const GURL& url,
      const std::vector<content::GlobalFrameRoutingId>& render_frames,
      base::OnceCallback<void(bool)> callback,
      bool allow);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  void GuestPermissionRequestHelper(
      const GURL& url,
      const std::vector<content::GlobalFrameRoutingId>& render_frames,
      base::OnceCallback<void(bool)> callback,
      bool allow);
#endif

  // The value pointed to by |settings| should remain valid until the
  // the function is called again with a new value or a nullptr.
  static void SetDefaultQuotaSettingsForTesting(
      const storage::QuotaSettings *settings);

  scoped_refptr<safe_browsing::UrlCheckerDelegate>
  GetSafeBrowsingUrlCheckerDelegate(content::ResourceContext* resource_context);

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

  scoped_refptr<safe_browsing::SafeBrowsingService> safe_browsing_service_;
  scoped_refptr<safe_browsing::UrlCheckerDelegate>
      safe_browsing_url_checker_delegate_;

  std::unique_ptr<data_reduction_proxy::DataReductionProxyThrottleManager>
      data_reduction_proxy_throttle_manager_;

  std::unique_ptr<service_manager::BinderRegistry> frame_interfaces_;
  std::unique_ptr<
      service_manager::BinderRegistryWithArgs<content::RenderFrameHost*>>
      frame_interfaces_parameterized_;
  std::unique_ptr<
      service_manager::BinderRegistryWithArgs<content::RenderProcessHost*,
                                              const url::Origin&>>
      worker_interfaces_parameterized_;

  StartupData* startup_data_;

#if !defined(OS_ANDROID)
  std::unique_ptr<ChromeSerialDelegate> serial_delegate_;
  std::unique_ptr<ChromeHidDelegate> hid_delegate_;
#endif

  // Returned from GetNetworkContextsParentDirectory() but created on the UI
  // thread because it needs to access the Local State prefs.
  std::vector<base::FilePath> network_contexts_parent_directory_;

  base::WeakPtrFactory<ChromeContentBrowserClient> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ChromeContentBrowserClient);
};

#endif  // CHROME_BROWSER_CHROME_CONTENT_BROWSER_CLIENT_H_
