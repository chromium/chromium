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
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/startup_data.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"

class ChromeContentBrowserClientParts;
class PrefRegistrySimple;
class ScopedKeepAlive;

namespace base {
class CommandLine;
}

namespace blink {
namespace mojom {
class WindowFeatures;
class WebUsbService;
}  // namespace mojom
namespace web_pref {
struct WebPreferences;
}  // namespace web_pref
class URLLoaderThrottle;
}  // namespace blink

namespace content {
class BrowserContext;
class FontAccessDelegate;
class QuotaPermissionContext;
enum class SmsFetchFailureType;
}  // namespace content

namespace safe_browsing {
class RealTimeUrlLookupServiceBase;
class SafeBrowsingService;
class UrlCheckerDelegate;
}  // namespace safe_browsing

namespace sandbox {
class SeatbeltExecClient;
}  // namespace sandbox

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

namespace net {
class IsolationInfo;
}

class ChromeBluetoothDelegate;
class ChromeFontAccessDelegate;
class ChromeHidDelegate;
class ChromeSerialDelegate;
class ChromeWebAuthenticationDelegate;

#if BUILDFLAG(ENABLE_VR)
namespace vr {
class ChromeXrIntegrationClient;
}
#endif

class ChromeContentBrowserClient : public content::ContentBrowserClient {
 public:
  ChromeContentBrowserClient();
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
  void PostAfterStartupTask(
      const base::Location& from_here,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      base::OnceClosure task) override;
  bool IsBrowserStartupComplete() override;
  void SetBrowserStartupIsCompleteForTesting() override;
  content::StoragePartitionId GetStoragePartitionIdForSite(
      content::BrowserContext* browser_context,
      const GURL& site) override;
  bool IsShuttingDown() override;
  content::StoragePartitionConfig GetStoragePartitionConfigForSite(
      content::BrowserContext* browser_context,
      const GURL& site) override;
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
                               const GURL& site_url) override;
  bool ShouldUseSpareRenderProcessHost(content::BrowserContext* browser_context,
                                       const GURL& site_url) override;
  bool DoesSiteRequireDedicatedProcess(content::BrowserContext* browser_context,
                                       const GURL& effective_site_url) override;
  bool ShouldLockProcessToSite(content::BrowserContext* browser_context,
                               const GURL& effective_site_url) override;
  bool DoesWebUISchemeRequireProcessLock(base::StringPiece scheme) override;
  bool ShouldTreatURLSchemeAsFirstPartyWhenTopLevel(
      base::StringPiece scheme,
      bool is_embedded_origin_secure) override;
  bool ShouldIgnoreSameSiteCookieRestrictionsWhenTopLevel(
      base::StringPiece scheme,
      bool is_embedded_origin_secure) override;
  std::string GetSiteDisplayNameForCdmProcess(
      content::BrowserContext* browser_context,
      const GURL& site_url) override;
  void OverrideURLLoaderFactoryParams(
      content::BrowserContext* browser_context,
      const url::Origin& origin,
      bool is_for_isolated_world,
      network::mojom::URLLoaderFactoryParams* factory_params) override;
  void GetAdditionalWebUISchemes(
      std::vector<std::string>* additional_schemes) override;
  void GetAdditionalViewSourceSchemes(
      std::vector<std::string>* additional_schemes) override;
  network::mojom::IPAddressSpace DetermineAddressSpaceFromURL(
      const GURL& url) override;
  bool LogWebUIUrl(const GURL& web_ui_url) override;
  bool IsWebUIAllowedToMakeNetworkRequests(const url::Origin& origin) override;
  bool IsHandledURL(const GURL& url) override;
  bool HasCustomSchemeHandler(content::BrowserContext* browser_context,
                              const std::string& scheme) override;
  bool CanCommitURL(content::RenderProcessHost* process_host,
                    const GURL& url) override;
  void OverrideNavigationParams(
      content::WebContents* web_contents,
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
  std::string GetApplicationClientGUIDForQuarantineCheck() override;
  download::QuarantineConnectionCallback GetQuarantineConnectionCallback()
      override;
  std::string GetApplicationLocale() override;
  std::string GetAcceptLangs(content::BrowserContext* context) override;
  gfx::ImageSkia GetDefaultFavicon() override;
  bool IsDataSaverEnabled(content::BrowserContext* context) override;
  void UpdateRendererPreferencesForWorker(
      content::BrowserContext* browser_context,
      blink::RendererPreferences* out_prefs) override;
  bool AllowAppCache(const GURL& manifest_url,

                     const GURL& site_for_cookies,
                     const base::Optional<url::Origin>& top_frame_origin,
                     content::BrowserContext* context) override;
  content::AllowServiceWorkerResult AllowServiceWorker(
      const GURL& scope,
      const GURL& site_for_cookies,
      const base::Optional<url::Origin>& top_frame_origin,
      const GURL& script_url,
      content::BrowserContext* context) override;
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
  bool IsInterestGroupAPIAllowed(content::BrowserContext* browser_context,
                                 const url::Origin& top_frame_origin,
                                 const GURL& api_url) override;
  bool IsConversionMeasurementAllowed(
      content::BrowserContext* browser_context) override;
  bool IsConversionMeasurementOperationAllowed(
      content::BrowserContext* browser_context,
      ConversionMeasurementOperation operation,
      const url::Origin* impression_origin,
      const url::Origin* conversion_origin,
      const url::Origin* reporting_origin) override;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  void OnTrustAnchorUsed(content::BrowserContext* browser_context) override;
#endif
  scoped_refptr<network::SharedURLLoaderFactory>
  GetSystemSharedURLLoaderFactory() override;
  network::mojom::NetworkContext* GetSystemNetworkContext() override;
  std::string GetGeolocationApiKey() override;
  device::GeolocationSystemPermissionManager* GetLocationPermissionManager()
      override;

#if defined(OS_ANDROID)
  bool ShouldUseGmsCoreGeolocationProvider() override;
#endif
  scoped_refptr<content::QuotaPermissionContext> CreateQuotaPermissionContext()
      override;
  content::GeneratedCodeCacheSettings GetGeneratedCodeCacheSettings(
      content::BrowserContext* context) override;
  void AllowCertificateError(
      content::WebContents* web_contents,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      bool is_main_frame_request,
      bool strict_enforcement,
      base::OnceCallback<void(content::CertificateRequestResultType)> callback)
      override;
#if !defined(OS_ANDROID)
  bool ShouldDenyRequestOnCertificateError(const GURL main_page_url) override;
#endif
  base::OnceClosure SelectClientCertificate(
      content::WebContents* web_contents,
      net::SSLCertRequestInfo* cert_request_info,
      net::ClientCertIdentityList client_certs,
      std::unique_ptr<content::ClientCertificateDelegate> delegate) override;
  content::MediaObserver* GetMediaObserver() override;
  content::FeatureObserverClient* GetFeatureObserverClient() override;
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
#if BUILDFLAG(IS_CHROMEOS_ASH)
  content::TtsControllerDelegate* GetTtsControllerDelegate() override;
#endif
  content::TtsPlatform* GetTtsPlatform() override;
  void OverrideWebkitPrefs(content::WebContents* web_contents,
                           blink::web_pref::WebPreferences* prefs) override;
  bool OverrideWebPreferencesAfterNavigation(
      content::WebContents* web_contents,
      blink::web_pref::WebPreferences* prefs) override;
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
  void GetSchemesBypassingSecureContextCheckAllowlist(
      std::set<std::string>* schemes) override;
  void GetURLRequestAutoMountHandlers(
      std::vector<storage::URLRequestAutoMountHandler>* handlers) override;
  void GetAdditionalFileSystemBackends(
      content::BrowserContext* browser_context,
      const base::FilePath& storage_partition_path,
      std::vector<std::unique_ptr<storage::FileSystemBackend>>*
          additional_backends) override;
  std::unique_ptr<content::DevToolsManagerDelegate>
  CreateDevToolsManagerDelegate() override;
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
#if defined(OS_POSIX) && !defined(OS_MAC)
  void GetAdditionalMappedFilesForChildProcess(
      const base::CommandLine& command_line,
      int child_process_id,
      content::PosixFileDescriptorInfo* mappings) override;
#endif  // defined(OS_POSIX) && !defined(OS_MAC)
#if defined(OS_WIN)
  bool PreSpawnChild(sandbox::TargetPolicy* policy,
                     sandbox::policy::SandboxType sandbox_type,
                     ChildSpawnFlags flags) override;
  std::wstring GetAppContainerSidForSandboxType(
      sandbox::policy::SandboxType sandbox_type) override;
  bool IsUtilityCetCompatible(const std::string& utility_sub_type) override;
  bool IsRendererCodeIntegrityEnabled() override;
  void SessionEnding() override;
  bool ShouldEnableAudioProcessHighPriority() override;
#endif
  void ExposeInterfacesToRenderer(
      service_manager::BinderRegistry* registry,
      blink::AssociatedInterfaceRegistry* associated_registry,
      content::RenderProcessHost* render_process_host) override;
  void BindMediaServiceReceiver(content::RenderFrameHost* render_frame_host,
                                mojo::GenericPendingReceiver receiver) override;
  void RegisterBrowserInterfaceBindersForFrame(
      content::RenderFrameHost* render_frame_host,
      mojo::BinderMapWithContext<content::RenderFrameHost*>* map) override;
  void RegisterMojoBinderPoliciesForSameOriginPrerendering(
      content::MojoBinderPolicyMap& policy_map) override;
  bool BindAssociatedReceiverFromFrame(
      content::RenderFrameHost* render_frame_host,
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle* handle) override;
  void BindBadgeServiceReceiverFromServiceWorker(
      content::RenderProcessHost* service_worker_process_host,
      const GURL& service_worker_scope,
      mojo::PendingReceiver<blink::mojom::BadgeService> receiver) override;
  void BindGpuHostReceiver(mojo::GenericPendingReceiver receiver) override;
  void BindUtilityHostReceiver(mojo::GenericPendingReceiver receiver) override;
  void BindHostReceiverForRenderer(
      content::RenderProcessHost* render_process_host,
      mojo::GenericPendingReceiver receiver) override;
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
  std::vector<std::unique_ptr<content::NavigationThrottle>>
  CreateThrottlesForNavigation(content::NavigationHandle* handle) override;
  std::unique_ptr<content::NavigationUIData> GetNavigationUIData(
      content::NavigationHandle* navigation_handle) override;
  void GetHardwareSecureDecryptionCaps(
      const std::string& key_system,
      base::flat_set<media::VideoCodec>* video_codecs,
      base::flat_set<media::EncryptionScheme>* encryption_schemes) override;
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
      ukm::SourceIdObj ukm_source_id,
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
      base::Optional<int64_t> navigation_id,
      ukm::SourceIdObj ukm_source_id,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>* factory_receiver,
      mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
          header_client,
      bool* bypass_redirect_checks,
      bool* disable_secure_dns,
      network::mojom::URLLoaderFactoryOverridePtr* factory_override) override;
  std::vector<std::unique_ptr<content::URLLoaderRequestInterceptor>>
  WillCreateURLLoaderRequestInterceptors(
      content::NavigationUIData* navigation_ui_data,
      int frame_tree_node_id,
      const scoped_refptr<network::SharedURLLoaderFactory>&
          network_loader_factory) override;
  content::ContentBrowserClient::URLLoaderRequestHandler
  CreateURLLoaderHandlerForServiceWorkerNavigationPreload(
      int frame_tree_node_id,
      const network::ResourceRequest& resource_request) override;
  bool WillInterceptWebSocket(content::RenderFrameHost* frame) override;
  void CreateWebSocket(
      content::RenderFrameHost* frame,
      WebSocketFactory factory,
      const GURL& url,
      const net::SiteForCookies& site_for_cookies,
      const base::Optional<std::string>& user_agent,
      mojo::PendingRemote<network::mojom::WebSocketHandshakeClient>
          handshake_client) override;
  bool WillCreateRestrictedCookieManager(
      network::mojom::RestrictedCookieManagerRole role,
      content::BrowserContext* browser_context,
      const url::Origin& origin,
      const net::IsolationInfo& isolation_info,
      bool is_service_worker,
      int process_id,
      int routing_id,
      mojo::PendingReceiver<network::mojom::RestrictedCookieManager>* receiver)
      override;
  void OnNetworkServiceCreated(
      network::mojom::NetworkService* network_service) override;
  void ConfigureNetworkContextParams(
      content::BrowserContext* context,
      bool in_memory,
      const base::FilePath& relative_partition_path,
      network::mojom::NetworkContextParams* network_context_params,
      cert_verifier::mojom::CertVerifierCreationParams*
          cert_verifier_creation_params) override;
  std::vector<base::FilePath> GetNetworkContextsParentDirectory() override;
  base::DictionaryValue GetNetLogConstants() override;
  bool AllowRenderingMhtmlOverHttp(
      content::NavigationUIData* navigation_ui_data) override;
  bool ShouldForceDownloadResource(const GURL& url,
                                   const std::string& mime_type) override;
  void CreateWebUsbService(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::WebUsbService> receiver) override;
  content::BluetoothDelegate* GetBluetoothDelegate() override;
#if !defined(OS_ANDROID)
  void CreateDeviceInfoService(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::DeviceAPIService> receiver) override;
  void CreateManagedConfigurationService(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::ManagedConfigurationService> receiver)
      override;
  content::SerialDelegate* GetSerialDelegate() override;
  content::HidDelegate* GetHidDelegate() override;
  content::FontAccessDelegate* GetFontAccessDelegate() override;
  content::WebAuthenticationDelegate* GetWebAuthenticationDelegate() override;
  std::unique_ptr<content::AuthenticatorRequestClientDelegate>
  GetWebAuthenticationRequestDelegate(
      content::RenderFrameHost* render_frame_host) override;
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
      content::WebContents::OnceGetter web_contents_getter,
      int child_id,
      int frame_tree_node_id,
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
  void OnNetworkServiceDataUseUpdate(int process_id,
                                     int routing_id,
                                     int32_t network_traffic_annotation_id_hash,
                                     int64_t recv_bytes,
                                     int64_t sent_bytes) override;
  base::FilePath GetSandboxedStorageServiceDataDirectory() override;
  bool ShouldSandboxAudioService() override;

  void LogWebFeatureForCurrentPage(content::RenderFrameHost* render_frame_host,
                                   blink::mojom::WebFeature feature) override;

  std::string GetProduct() override;
  std::string GetUserAgent() override;
  blink::UserAgentMetadata GetUserAgentMetadata() override;

  base::Optional<gfx::ImageSkia> GetProductLogo() override;

  bool IsBuiltinComponent(content::BrowserContext* browser_context,
                          const url::Origin& origin) override;

  bool ShouldBlockRendererDebugURL(const GURL& url,
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
      content::WebContents* web_contents,
      content::RenderFrameHost* frame_host,
      bool user_gesture,
      blink::NavigationDownloadPolicy* download_policy) override;

  blink::mojom::InterestCohortPtr GetInterestCohortForJsApi(
      content::WebContents* web_contents,
      const GURL& url,
      const base::Optional<url::Origin>& top_frame_origin) override;

  bool IsBluetoothScanningBlocked(content::BrowserContext* browser_context,
                                  const url::Origin& requesting_origin,
                                  const url::Origin& embedding_origin) override;

  void BlockBluetoothScanning(content::BrowserContext* browser_context,
                              const url::Origin& requesting_origin,
                              const url::Origin& embedding_origin) override;

  bool ShouldLoadExtraIcuDataFile(std::string* split_name) override;

  bool ArePersistentMediaDeviceIDsAllowed(
      content::BrowserContext* browser_context,
      const GURL& scope,
      const GURL& site_for_cookies,
      const base::Optional<url::Origin>& top_frame_origin) override;

#if !defined(OS_ANDROID)
  base::OnceClosure FetchRemoteSms(
      content::WebContents* web_contents,
      const url::Origin& origin,
      base::OnceCallback<void(base::Optional<std::vector<url::Origin>>,
                              base::Optional<std::string>,
                              base::Optional<content::SmsFetchFailureType>)>
          callback) override;
#endif

  bool IsClipboardPasteAllowed(
      content::RenderFrameHost* render_frame_host) override;

  void IsClipboardPasteContentAllowed(
      content::WebContents* web_contents,
      const GURL& url,
      const ui::ClipboardFormatType& data_type,
      const std::string& data,
      IsClipboardPasteContentAllowedCallback callback) override;

#if BUILDFLAG(ENABLE_PLUGINS)
  bool ShouldAllowPluginCreation(
      const url::Origin& embedder_origin,
      const content::PepperPluginInfo& plugin_info) override;
#endif

#if BUILDFLAG(ENABLE_VR)
  content::XrIntegrationClient* GetXrIntegrationClient() override;
#endif

  bool IsOriginTrialRequiredForAppCache(
      content::BrowserContext* browser_context) override;
  void BindBrowserControlInterface(mojo::ScopedMessagePipeHandle pipe) override;
  bool ShouldInheritCrossOriginEmbedderPolicyImplicitly(
      const GURL& url) override;
  bool ShouldAllowInsecurePrivateNetworkRequests(
      content::BrowserContext* browser_context,
      const url::Origin& origin) override;
  ukm::UkmService* GetUkmService() override;

  void OnKeepaliveRequestStarted(
      content::BrowserContext* browser_context) override;
  void OnKeepaliveRequestFinished() override;

#if defined(OS_MAC)
  bool SetupEmbedderSandboxParameters(
      sandbox::policy::SandboxType sandbox_type,
      sandbox::SeatbeltExecClient* client) override;
#endif  // defined(OS_MAC)

  void GetHyphenationDictionary(
      base::OnceCallback<void(const base::FilePath&)>) override;
  bool HasErrorPage(int http_status_code) override;

  StartupData* startup_data() { return &startup_data_; }

  std::unique_ptr<content::IdentityRequestDialogController>
  CreateIdentityRequestDialogController() override;

#if !defined(OS_ANDROID)
  base::TimeDelta GetKeepaliveTimerTimeout(content::BrowserContext* context);
#endif  // !defined(OS_ANDROID)

  bool SuppressDifferentOriginSubframeJSDialogs(
      content::BrowserContext* browser_context) override;

 protected:
  static bool HandleWebUI(GURL* url, content::BrowserContext* browser_context);
  static bool HandleWebUIReverse(GURL* url,
                                 content::BrowserContext* browser_context);
  virtual const ui::NativeTheme* GetWebTheme() const;  // For testing.

  // Used by subclasses (e.g. implemented by downstream embedders) to add
  // their own extra part objects.
  void AddExtraPart(ChromeContentBrowserClientParts* part) {
    extra_parts_.push_back(part);
  }

 private:
  friend class DisableWebRtcEncryptionFlagTest;
  friend class InProcessBrowserTest;

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

  // Returns the existing UrlCheckerDelegate object if it is already created.
  // Otherwise, creates a new one and returns it. Updates the
  // |allowlist_domains| in the UrlCheckerDelegate object before returning. It
  // returns nullptr if |safe_browsing_enabled_for_profile| is false, because it
  // should bypass safe browsing check when safe browsing is disabled. Set
  // |should_check_on_sb_disabled| to true if you still want to perform safe
  // browsing check when safe browsing is disabled(e.g. for enterprise real time
  // URL check).
  scoped_refptr<safe_browsing::UrlCheckerDelegate>
  GetSafeBrowsingUrlCheckerDelegate(
      bool safe_browsing_enabled_for_profile,
      bool should_check_on_sb_disabled,
      const std::vector<std::string>& allowlist_domains);

  // Returns a RealTimeUrlLookupServiceBase object used for real time URL check.
  // Returns an enterprise version if |is_enterprise_lookup_enabled| is true.
  // Returns a consumer version if |is_enterprise_lookup_enabled| is false and
  // |is_consumer_lookup_enabled| is true. Returns nullptr if both are false.
  safe_browsing::RealTimeUrlLookupServiceBase* GetUrlLookupService(
      content::BrowserContext* browser_context,
      bool is_enterprise_lookup_enabled,
      bool is_consumer_lookup_enabled);

#if !defined(OS_ANDROID)
  void OnKeepaliveTimerFired(
      std::unique_ptr<ScopedKeepAlive> keep_alive_handle);
#endif

  // Vector of additional ChromeContentBrowserClientParts.
  // Parts are deleted in the reverse order they are added.
  std::vector<ChromeContentBrowserClientParts*> extra_parts_;

  scoped_refptr<safe_browsing::SafeBrowsingService> safe_browsing_service_;
  scoped_refptr<safe_browsing::UrlCheckerDelegate>
      safe_browsing_url_checker_delegate_;

  StartupData startup_data_;

#if !defined(OS_ANDROID)
  std::unique_ptr<ChromeSerialDelegate> serial_delegate_;
  std::unique_ptr<ChromeHidDelegate> hid_delegate_;
  std::unique_ptr<ChromeFontAccessDelegate> font_access_delegate_;
  std::unique_ptr<ChromeWebAuthenticationDelegate> web_authentication_delegate_;
#endif
  std::unique_ptr<ChromeBluetoothDelegate> bluetooth_delegate_;

#if BUILDFLAG(ENABLE_VR)
  std::unique_ptr<vr::ChromeXrIntegrationClient> xr_integration_client_;
#endif

  // Returned from GetNetworkContextsParentDirectory() but created on the UI
  // thread because it needs to access the Local State prefs.
  std::vector<base::FilePath> network_contexts_parent_directory_;

#if !defined(OS_ANDROID)
  uint64_t num_keepalive_requests_ = 0;
  base::OneShotTimer keepalive_timer_;
  base::TimeTicks keepalive_deadline_;
#endif

  base::WeakPtrFactory<ChromeContentBrowserClient> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ChromeContentBrowserClient);
};

#endif  // CHROME_BROWSER_CHROME_CONTENT_BROWSER_CLIENT_H_
