// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_CONTENT_BROWSER_CLIENT_H_
#define CHROME_BROWSER_CHROME_CONTENT_BROWSER_CLIENT_H_

#include <stddef.h>

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/startup_data.h"
#include "components/file_access/scoped_file_access.h"
#include "components/safe_browsing/buildflags.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/digital_identity_provider.h"
#include "content/public/browser/legacy_tech_cookie_issue_details.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/alternative_error_page_override_info.mojom-forward.h"
#include "extensions/buildflags/buildflags.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/network_handle.h"
#include "services/device/public/cpp/geolocation/buildflags.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom-forward.h"
#include "third_party/blink/public/mojom/worker/shared_worker_info.mojom.h"

class ChromeContentBrowserClientParts;
class PrefRegistrySimple;
class ScopedKeepAlive;

namespace base {
class CommandLine;
}  // namespace base

namespace blink {
namespace mojom {
class WindowFeatures;
}  // namespace mojom
namespace web_pref {
struct WebPreferences;
}  // namespace web_pref
class StorageKey;
class URLLoaderThrottle;
}  // namespace blink

namespace blocked_content {
class PopupNavigationDelegate;
}  // namespace blocked_content

namespace content {
class BrowserContext;
class RenderFrameHost;
enum class SmsFetchFailureType;
struct ServiceWorkerVersionBaseInfo;
}  // namespace content

namespace net {
class IsolationInfo;
class SiteForCookies;
}  // namespace net

namespace safe_browsing {
class AsyncCheckTracker;
class RealTimeUrlLookupServiceBase;
class SafeBrowsingService;
class UrlCheckerDelegate;

namespace hash_realtime_utils {
enum class HashRealTimeSelection;
}
}  // namespace safe_browsing

namespace sandbox {
class SandboxCompiler;
}  // namespace sandbox

namespace ui {
class NativeTheme;
}  // namespace ui

namespace url {
class Origin;
}  // namespace url

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace version_info {
enum class Channel;
}  // namespace version_info

class ChromeDirectSocketsDelegate;
class ChromeHidDelegate;
class ChromePrivateNetworkDeviceDelegate;
class ChromeSerialDelegate;
class ChromeBluetoothDelegate;
class ChromeUsbDelegate;
class ChromeWebAuthenticationDelegate;
class HttpAuthCoordinator;
class MainThreadStackSamplingProfiler;
struct NavigateParams;

#if BUILDFLAG(ENABLE_VR)
namespace vr {
class ChromeXrIntegrationClient;
}
#endif

class ChromeContentBrowserClient : public content::ContentBrowserClient {
 public:
  using PopupNavigationDelegateFactory =
      std::unique_ptr<blocked_content::PopupNavigationDelegate> (*)(
          NavigateParams);
  using ClipboardPasteData = content::ClipboardPasteData;

  static PopupNavigationDelegateFactory&
  GetPopupNavigationDelegateFactoryForTesting();

  ChromeContentBrowserClient();

  ChromeContentBrowserClient(const ChromeContentBrowserClient&) = delete;
  ChromeContentBrowserClient& operator=(const ChromeContentBrowserClient&) =
      delete;

  ~ChromeContentBrowserClient() override;

  // TODO(crbug.com/41356866): This file is about calls from content/ out
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
      bool is_integration_test) override;
  void PostAfterStartupTask(
      const base::Location& from_here,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      base::OnceClosure task) override;
  bool IsBrowserStartupComplete() override;
  void SetBrowserStartupIsCompleteForTesting() override;
  bool IsShuttingDown() override;
  void ThreadPoolWillTerminate() override;
  content::StoragePartitionConfig GetStoragePartitionConfigForSite(
      content::BrowserContext* browser_context,
      const GURL& site) override;
  std::unique_ptr<content::WebContentsViewDelegate> GetWebContentsViewDelegate(
      content::WebContents* web_contents) override;
  void RenderProcessWillLaunch(content::RenderProcessHost* host) override;
  bool AllowGpuLaunchRetryOnIOThread() override;
  GURL GetEffectiveURL(content::BrowserContext* browser_context,
                       const GURL& url) override;
  bool ShouldCompareEffectiveURLsForSiteInstanceSelection(
      content::BrowserContext* browser_context,
      content::SiteInstance* candidate_site_instance,
      bool is_outermost_main_frame,
      const GURL& candidate_url,
      const GURL& destination_url) override;
  bool ShouldUseProcessPerSite(content::BrowserContext* browser_context,
                               const GURL& site_url) override;
  bool ShouldAllowProcessPerSiteForMultipleMainFrames(
      content::BrowserContext* context) override;
  std::optional<SpareProcessRefusedByEmbedderReason>
  ShouldUseSpareRenderProcessHost(content::BrowserContext* browser_context,
                                  const GURL& site_url) override;
  bool DoesSiteRequireDedicatedProcess(content::BrowserContext* browser_context,
                                       const GURL& effective_site_url) override;
  bool ShouldAllowCrossProcessSandboxedFrameForPrecursor(
      content::BrowserContext* browser_context,
      const GURL& precursor,
      const GURL& url) override;
  bool DoesWebUIUrlRequireProcessLock(const GURL& url) override;
  bool ShouldTreatURLSchemeAsFirstPartyWhenTopLevel(
      std::string_view scheme,
      bool is_embedded_origin_secure) override;
  bool ShouldIgnoreSameSiteCookieRestrictionsWhenTopLevel(
      std::string_view scheme,
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
  bool HasWebRequestAPIProxy(content::BrowserContext* browser_context) override;
  bool CanCommitURL(content::RenderProcessHost* process_host,
                    const GURL& url) override;
  void OverrideNavigationParams(
      std::optional<GURL> source_process_site_url,
      ui::PageTransition* transition,
      bool* is_renderer_initiated,
      content::Referrer* referrer,
      std::optional<url::Origin>* initiator_origin) override;
  bool ShouldStayInParentProcessForNTP(const GURL& url,
                                       const GURL& parent_site_url) override;
  bool IsSuitableHost(content::RenderProcessHost* process_host,
                      const GURL& site_url) override;
  bool MayReuseHost(content::RenderProcessHost* process_host) override;
  size_t GetProcessCountToIgnoreForLimit() override;
  std::optional<blink::ParsedPermissionsPolicy>
  GetPermissionsPolicyForIsolatedWebApp(content::WebContents* web_contents,
                                        const url::Origin& app_origin) override;
  bool ShouldTryToUseExistingProcessHost(
      content::BrowserContext* browser_context,
      const GURL& url) override;
  bool ShouldEmbeddedFramesTryToReuseExistingProcess(
      content::RenderFrameHost* outermost_main_frame) override;
  void SiteInstanceGotProcessAndSite(
      content::SiteInstance* site_instance) override;
  bool ShouldSwapBrowsingInstancesForNavigation(
      content::SiteInstance* site_instance,
      const GURL& current_effective_url,
      const GURL& destination_effective_url) override;
  bool ShouldIsolateErrorPage(bool in_main_frame) override;
  std::vector<url::Origin> GetOriginsRequiringDedicatedProcess() override;
  bool ShouldEnableStrictSiteIsolation() override;
  bool ShouldDisableSiteIsolation(
      content::SiteIsolationMode site_isolation_mode) override;
  std::vector<std::string> GetAdditionalSiteIsolationModes() override;
  void PersistIsolatedOrigin(
      content::BrowserContext* context,
      const url::Origin& origin,
      content::ChildProcessSecurityPolicy::IsolatedOriginSource source)
      override;
  bool ShouldUrlUseApplicationIsolationLevel(
      content::BrowserContext* browser_context,
      const GURL& url) override;
  bool IsIsolatedContextAllowedForUrl(content::BrowserContext* browser_context,
                                      const GURL& lock_url) override;
  void CheckGetAllScreensMediaAllowed(
      content::RenderFrameHost* render_frame_host,
      base::OnceCallback<void(bool)> callback) override;
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
  content::AllowServiceWorkerResult AllowServiceWorker(
      const GURL& scope,
      const net::SiteForCookies& site_for_cookies,
      const std::optional<url::Origin>& top_frame_origin,
      const GURL& script_url,
      content::BrowserContext* context) override;
  bool MayDeleteServiceWorkerRegistration(
      const GURL& scope,
      content::BrowserContext* browser_context) override;
  bool ShouldTryToUpdateServiceWorkerRegistration(
      const GURL& scope,
      content::BrowserContext* browser_context) override;
  bool AllowSharedWorker(
      const GURL& worker_url,
      const net::SiteForCookies& site_for_cookies,
      const std::optional<url::Origin>& top_frame_origin,
      const std::string& name,
      const blink::StorageKey& storage_key,
      const blink::mojom::SharedWorkerSameSiteCookies same_site_cookies,
      content::BrowserContext* context,
      int render_process_id,
      int render_frame_id) override;
  bool DoesSchemeAllowCrossOriginSharedWorker(
      const std::string& scheme) override;
  bool AllowSignedExchange(content::BrowserContext* browser_context) override;
  bool AllowCompressionDictionaryTransport(
      content::BrowserContext* context) override;
  void RequestFilesAccess(
      const std::vector<base::FilePath>& files,
      const GURL& destination_url,
      base::OnceCallback<void(file_access::ScopedFileAccess)>
          continuation_callback) override;
  void AllowWorkerFileSystem(
      const GURL& url,
      content::BrowserContext* browser_context,
      const std::vector<content::GlobalRenderFrameHostId>& render_frames,
      base::OnceCallback<void(bool)> callback) override;
  bool AllowWorkerIndexedDB(const GURL& url,
                            content::BrowserContext* browser_context,
                            const std::vector<content::GlobalRenderFrameHostId>&
                                render_frames) override;
  bool AllowWorkerCacheStorage(
      const GURL& url,
      content::BrowserContext* browser_context,
      const std::vector<content::GlobalRenderFrameHostId>& render_frames)
      override;
  bool AllowWorkerWebLocks(const GURL& url,
                           content::BrowserContext* browser_context,
                           const std::vector<content::GlobalRenderFrameHostId>&
                               render_frames) override;
  AllowWebBluetoothResult AllowWebBluetooth(
      content::BrowserContext* browser_context,
      const url::Origin& requesting_origin,
      const url::Origin& embedding_origin) override;
  std::string GetWebBluetoothBlocklist() override;
  bool IsInterestGroupAPIAllowed(content::RenderFrameHost* render_frame_host,
                                 InterestGroupApiOperation operation,
                                 const url::Origin& top_frame_origin,
                                 const url::Origin& api_origin) override;
  bool IsPrivacySandboxReportingDestinationAttested(
      content::BrowserContext* browser_context,
      const url::Origin& destination_origin,
      content::PrivacySandboxInvokingAPI invoking_api) override;
  void OnAuctionComplete(
      content::RenderFrameHost* render_frame_host,
      std::optional<content::InterestGroupManager::InterestGroupDataKey>
          winner_data_key,
      bool is_server_auction,
      bool is_on_device_auction,
      content::AuctionResult result) override;
  bool IsAttributionReportingOperationAllowed(
      content::BrowserContext* browser_context,
      AttributionReportingOperation operation,
      content::RenderFrameHost* rfh,
      const url::Origin* impression_origin,
      const url::Origin* conversion_origin,
      const url::Origin* reporting_origin,
      bool* can_bypass) override;
  bool IsAttributionReportingAllowedForContext(
      content::BrowserContext* browser_context,
      content::RenderFrameHost* rfh,
      const url::Origin& context_origin,
      const url::Origin& reporting_origin) override;
  bool IsSharedStorageAllowed(
      content::BrowserContext* browser_context,
      content::RenderFrameHost* rfh,
      const url::Origin& top_frame_origin,
      const url::Origin& accessing_origin,
      std::string* out_debug_message = nullptr,
      bool* out_block_is_site_setting_specific = nullptr) override;
  bool IsSharedStorageSelectURLAllowed(
      content::BrowserContext* browser_context,
      const url::Origin& top_frame_origin,
      const url::Origin& accessing_origin,
      std::string* out_debug_message = nullptr,
      bool* out_block_is_site_setting_specific = nullptr) override;
  bool IsPrivateAggregationAllowed(
      content::BrowserContext* browser_context,
      const url::Origin& top_frame_origin,
      const url::Origin& reporting_origin,
      bool* out_block_is_site_setting_specific = nullptr) override;
  bool IsPrivateAggregationDebugModeAllowed(
      content::BrowserContext* browser_context,
      const url::Origin& top_frame_origin,
      const url::Origin& reporting_origin) override;
  bool IsCookieDeprecationLabelAllowed(
      content::BrowserContext* browser_context) override;
  bool IsCookieDeprecationLabelAllowedForContext(
      content::BrowserContext* browser_context,
      const url::Origin& top_frame_origin,
      const url::Origin& context_origin) override;
  bool IsFullCookieAccessAllowed(content::BrowserContext* browser_context,
                                 content::WebContents* web_contents,
                                 const GURL& url,
                                 const blink::StorageKey& storage_key) override;
  void GrantCookieAccessDueToHeuristic(content::BrowserContext* browser_context,
                                       const net::SchemefulSite& top_frame_site,
                                       const net::SchemefulSite& accessing_site,
                                       base::TimeDelta ttl,
                                       bool ignore_schemes) override;
#if BUILDFLAG(IS_CHROMEOS)
  void OnTrustAnchorUsed(content::BrowserContext* browser_context) override;
#endif
  bool CanSendSCTAuditingReport(
      content::BrowserContext* browser_context) override;
  void OnNewSCTAuditingReportSent(
      content::BrowserContext* browser_context) override;
  scoped_refptr<network::SharedURLLoaderFactory>
  GetSystemSharedURLLoaderFactory() override;
  network::mojom::NetworkContext* GetSystemNetworkContext() override;
  std::string GetGeolocationApiKey() override;

#if BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
  device::GeolocationSystemPermissionManager*
  GetGeolocationSystemPermissionManager() override;
#endif

#if BUILDFLAG(IS_ANDROID)
  bool ShouldUseGmsCoreGeolocationProvider() override;
#endif
  content::GeneratedCodeCacheSettings GetGeneratedCodeCacheSettings(
      content::BrowserContext* context) override;
  std::string GetWebUIHostnameForCodeCacheMetrics(
      const GURL& webui_url) const override;
  void AllowCertificateError(
      content::WebContents* web_contents,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      bool is_primary_main_frame_request,
      bool strict_enforcement,
      base::OnceCallback<void(content::CertificateRequestResultType)> callback)
      override;
#if !BUILDFLAG(IS_ANDROID)
  bool ShouldDenyRequestOnCertificateError(const GURL main_page_url) override;
#endif
  base::OnceClosure SelectClientCertificate(
      content::BrowserContext* browser_context,
      int process_id,
      content::WebContents* web_contents,
      net::SSLCertRequestInfo* cert_request_info,
      net::ClientCertIdentityList client_certs,
      std::unique_ptr<content::ClientCertificateDelegate> delegate) override;
  content::MediaObserver* GetMediaObserver() override;
  content::FeatureObserverClient* GetFeatureObserverClient() override;
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
  void MaybeOverrideManifest(content::RenderFrameHost* render_frame_host,
                             blink::mojom::ManifestPtr& manifest) override;
  content::TtsPlatform* GetTtsPlatform() override;
  void OverrideWebkitPrefs(content::WebContents* web_contents,
                           blink::web_pref::WebPreferences* prefs) override;
  bool OverrideWebPreferencesAfterNavigation(
      content::WebContents* web_contents,
      blink::web_pref::WebPreferences* prefs) override;
  void BrowserURLHandlerCreated(content::BrowserURLHandler* handler) override;
  base::FilePath GetDefaultDownloadDirectory() override;
  std::string GetDefaultDownloadName() override;
  base::FilePath GetShaderDiskCacheDirectory() override;
  base::FilePath GetGrShaderDiskCacheDirectory() override;
  base::FilePath GetGraphiteDawnDiskCacheDirectory() override;
  base::FilePath GetNetLogDefaultDirectory() override;
  base::FilePath GetFirstPartySetsDirectory() override;
  std::optional<base::FilePath> GetLocalTracesDirectory() override;
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
  std::optional<base::TimeDelta> GetSpareRendererDelayForSiteURL(
      const GURL& site_url) override;
  std::unique_ptr<content::TracingDelegate> CreateTracingDelegate() override;
  bool IsSystemWideTracingEnabled() override;
  bool IsPluginAllowedToCallRequestOSFileHandle(
      content::BrowserContext* browser_context,
      const GURL& url) override;
  bool IsPluginAllowedToUseDevChannelAPIs(
      content::BrowserContext* browser_context,
      const GURL& url) override;
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
  void GetAdditionalMappedFilesForChildProcess(
      const base::CommandLine& command_line,
      int child_process_id,
      content::PosixFileDescriptorInfo* mappings) override;
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void GetAdditionalMappedFilesForZygote(
      base::CommandLine* command_line,
      content::PosixFileDescriptorInfo* mappings) override;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
#if BUILDFLAG(IS_WIN)
  bool PreSpawnChild(sandbox::TargetConfig* config,
                     sandbox::mojom::Sandbox sandbox_type,
                     ChildSpawnFlags flags) override;
  std::wstring GetAppContainerSidForSandboxType(
      sandbox::mojom::Sandbox sandbox_type,
      AppContainerFlags flags) override;
  bool IsAppContainerDisabled(sandbox::mojom::Sandbox sandbox_type) override;
  std::wstring GetLPACCapabilityNameForNetworkService() override;
  bool IsUtilityCetCompatible(const std::string& utility_sub_type) override;
  bool IsRendererCodeIntegrityEnabled() override;
  void SessionEnding(std::optional<DWORD> control_type) override;
  bool ShouldEnableAudioProcessHighPriority() override;
  bool ShouldUseSkiaFontManager(const GURL& site_url) override;
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
  void RegisterWebUIInterfaceBrokers(
      content::WebUIBrowserInterfaceBrokerRegistry& registry) override;
  void RegisterMojoBinderPoliciesForSameOriginPrerendering(
      content::MojoBinderPolicyMap& policy_map) override;
  void RegisterMojoBinderPoliciesForPreview(
      content::MojoBinderPolicyMap& policy_map) override;
  void RegisterBrowserInterfaceBindersForServiceWorker(
      content::BrowserContext* browser_context,
      const content::ServiceWorkerVersionBaseInfo& service_worker_version_info,
      mojo::BinderMapWithContext<const content::ServiceWorkerVersionBaseInfo&>*
          map) override;
  void RegisterAssociatedInterfaceBindersForServiceWorker(
      const content::ServiceWorkerVersionBaseInfo& service_worker_version_info,
      blink::AssociatedInterfaceRegistry& associated_registry) override;
  void RegisterAssociatedInterfaceBindersForRenderFrameHost(
      content::RenderFrameHost& render_frame_host,
      blink::AssociatedInterfaceRegistry& associated_registry) override;
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
  void AddPresentationObserver(content::PresentationObserver* observer,
                               content::WebContents* web_contents) override;
  void RemovePresentationObserver(content::PresentationObserver* observer,
                                  content::WebContents* web_contents) override;
  bool AddPrivacySandboxAttestationsObserver(
      content::PrivacySandboxAttestationsObserver* observer) override;
  void RemovePrivacySandboxAttestationsObserver(
      content::PrivacySandboxAttestationsObserver* observer) override;
  std::vector<std::unique_ptr<content::NavigationThrottle>>
  CreateThrottlesForNavigation(content::NavigationHandle* handle) override;
  std::vector<std::unique_ptr<content::CommitDeferringCondition>>
  CreateCommitDeferringConditionsForNavigation(
      content::NavigationHandle* navigation_handle,
      content::CommitDeferringCondition::NavigationType type) override;
  std::unique_ptr<content::NavigationUIData> GetNavigationUIData(
      content::NavigationHandle* navigation_handle) override;
  std::unique_ptr<media::ScreenEnumerator> CreateScreenEnumerator()
      const override;
  bool EnforceSystemAudioEchoCancellation() override;
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
      content::FrameTreeNodeId frame_tree_node_id,
      std::optional<int64_t> navigation_id) override;
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
  CreateURLLoaderThrottlesForKeepAlive(
      const network::ResourceRequest& request,
      content::BrowserContext* browser_context,
      const base::RepeatingCallback<content::WebContents*()>& wc_getter,
      content::FrameTreeNodeId frame_tree_node_id) override;
  mojo::PendingRemote<network::mojom::URLLoaderFactory>
  CreateNonNetworkNavigationURLLoaderFactory(
      const std::string& scheme,
      content::FrameTreeNodeId frame_tree_node_id) override;
  void RegisterNonNetworkWorkerMainResourceURLLoaderFactories(
      content::BrowserContext* browser_context,
      NonNetworkURLLoaderFactoryMap* factories) override;
  void RegisterNonNetworkServiceWorkerUpdateURLLoaderFactories(
      content::BrowserContext* browser_context,
      NonNetworkURLLoaderFactoryMap* factories) override;
  void RegisterNonNetworkSubresourceURLLoaderFactories(
      int render_process_id,
      int render_frame_id,
      const std::optional<url::Origin>& request_initiator_origin,
      NonNetworkURLLoaderFactoryMap* factories) override;
  void WillCreateURLLoaderFactory(
      content::BrowserContext* browser_context,
      content::RenderFrameHost* frame,
      int render_process_id,
      URLLoaderFactoryType type,
      const url::Origin& request_initiator,
      const net::IsolationInfo& isolation_info,
      std::optional<int64_t> navigation_id,
      ukm::SourceIdObj ukm_source_id,
      network::URLLoaderFactoryBuilder& factory_builder,
      mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
          header_client,
      bool* bypass_redirect_checks,
      bool* disable_secure_dns,
      network::mojom::URLLoaderFactoryOverridePtr* factory_override,
      scoped_refptr<base::SequencedTaskRunner> navigation_response_task_runner)
      override;
  std::vector<std::unique_ptr<content::URLLoaderRequestInterceptor>>
  WillCreateURLLoaderRequestInterceptors(
      content::NavigationUIData* navigation_ui_data,
      content::FrameTreeNodeId frame_tree_node_id,
      int64_t navigation_id,
      bool force_no_https_upgrade,
      scoped_refptr<base::SequencedTaskRunner> navigation_response_task_runner)
      override;
  content::ContentBrowserClient::URLLoaderRequestHandler
  CreateURLLoaderHandlerForServiceWorkerNavigationPreload(
      content::FrameTreeNodeId frame_tree_node_id,
      const network::ResourceRequest& resource_request) override;
  bool WillInterceptWebSocket(content::RenderFrameHost* frame) override;
  void CreateWebSocket(
      content::RenderFrameHost* frame,
      WebSocketFactory factory,
      const GURL& url,
      const net::SiteForCookies& site_for_cookies,
      const std::optional<std::string>& user_agent,
      mojo::PendingRemote<network::mojom::WebSocketHandshakeClient>
          handshake_client) override;
  void WillCreateWebTransport(
      int process_id,
      int frame_routing_id,
      const GURL& url,
      const url::Origin& initiator_origin,
      mojo::PendingRemote<network::mojom::WebTransportHandshakeClient>
          handshake_client,
      WillCreateWebTransportCallback callback) override;

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
  base::Value::Dict GetNetLogConstants() override;
  bool AllowRenderingMhtmlOverHttp(
      content::NavigationUIData* navigation_ui_data) override;
  bool ShouldForceDownloadResource(content::BrowserContext* browser_context,
                                   const GURL& url,
                                   const std::string& mime_type) override;
  content::BluetoothDelegate* GetBluetoothDelegate() override;
  content::UsbDelegate* GetUsbDelegate() override;
  content::PrivateNetworkDeviceDelegate* GetPrivateNetworkDeviceDelegate()
      override;
  bool IsSecurityLevelAcceptableForWebAuthn(
      content::RenderFrameHost* rfh,
      const url::Origin& caller_origin) override;
#if !BUILDFLAG(IS_ANDROID)
  void CreateDeviceInfoService(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::DeviceAPIService> receiver) override;
  void CreateManagedConfigurationService(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::ManagedConfigurationService> receiver)
      override;
  content::SerialDelegate* GetSerialDelegate() override;
  content::HidDelegate* GetHidDelegate() override;
  content::DirectSocketsDelegate* GetDirectSocketsDelegate() override;
  content::WebAuthenticationDelegate* GetWebAuthenticationDelegate() override;
  std::unique_ptr<content::AuthenticatorRequestClientDelegate>
  GetWebAuthenticationRequestDelegate(
      content::RenderFrameHost* render_frame_host) override;
#endif
  void CreatePaymentCredential(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<payments::mojom::PaymentCredential> receiver)
      override;
#if BUILDFLAG(IS_CHROMEOS)
  content::SmartCardDelegate* GetSmartCardDelegate() override;
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
      content::BrowserContext* browser_context,
      const content::GlobalRequestID& request_id,
      bool is_request_for_primary_main_frame,
      bool is_request_for_navigation,
      const GURL& url,
      scoped_refptr<net::HttpResponseHeaders> response_headers,
      bool first_auth_attempt,
      LoginAuthRequiredCallback auth_required_callback) override;
  bool HandleExternalProtocol(
      const GURL& url,
      content::WebContents::Getter web_contents_getter,
      content::FrameTreeNodeId frame_tree_node_id,
      content::NavigationUIData* navigation_data,
      bool is_primary_main_frame,
      bool is_in_fenced_frame_tree,
      network::mojom::WebSandboxFlags sandbox_flags,
      ui::PageTransition page_transition,
      bool has_user_gesture,
      const std::optional<url::Origin>& initiating_origin,
      content::RenderFrameHost* initiator_document,
      const net::IsolationInfo& isolation_info,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>* out_factory)
      override;
  std::unique_ptr<content::VideoOverlayWindow>
  CreateWindowForVideoPictureInPicture(
      content::VideoPictureInPictureWindowController* controller) override;
  void RegisterRendererPreferenceWatcher(
      content::BrowserContext* browser_context,
      mojo::PendingRemote<blink::mojom::RendererPreferenceWatcher> watcher)
      override;
  bool CanAcceptUntrustedExchangesIfNeeded() override;
  void OnNetworkServiceDataUseUpdate(
      content::GlobalRenderFrameHostId render_frame_host_id,
      int32_t network_traffic_annotation_id_hash,
      int64_t recv_bytes,
      int64_t sent_bytes) override;
  base::FilePath GetSandboxedStorageServiceDataDirectory() override;
  bool ShouldSandboxAudioService() override;
  bool ShouldSandboxNetworkService() override;
  bool ShouldRunOutOfProcessSystemDnsResolution() override;

  void LogWebFeatureForCurrentPage(content::RenderFrameHost* render_frame_host,
                                   blink::mojom::WebFeature feature) override;
  void LogWebDXFeatureForCurrentPage(
      content::RenderFrameHost* render_frame_host,
      blink::mojom::WebDXFeature feature) override;

  std::string GetProduct() override;
  std::string GetUserAgent() override;
  std::string GetUserAgentBasedOnPolicy(
      content::BrowserContext* context) override;
  blink::UserAgentMetadata GetUserAgentMetadata() override;

  std::optional<gfx::ImageSkia> GetProductLogo() override;

  bool IsBuiltinComponent(content::BrowserContext* browser_context,
                          const url::Origin& origin) override;

  bool ShouldBlockRendererDebugURL(
      const GURL& url,
      content::BrowserContext* context,
      content::RenderFrameHost* render_frame_host) override;

#if BUILDFLAG(IS_ANDROID)
  ContentBrowserClient::WideColorGamutHeuristic GetWideColorGamutHeuristic()
      override;
#endif

  base::flat_set<std::string> GetPluginMimeTypesWithExternalHandlers(
      content::BrowserContext* browser_context) override;

  void AugmentNavigationDownloadPolicy(
      content::RenderFrameHost* frame_host,
      bool user_gesture,
      blink::NavigationDownloadPolicy* download_policy) override;

  bool HandleTopicsWebApi(
      const url::Origin& context_origin,
      content::RenderFrameHost* main_frame,
      browsing_topics::ApiCallerSource caller_source,
      bool get_topics,
      bool observe,
      std::vector<blink::mojom::EpochTopicPtr>& topics) override;

  int NumVersionsInTopicsEpochs(
      content::RenderFrameHost* main_frame) const override;

  bool IsBluetoothScanningBlocked(content::BrowserContext* browser_context,
                                  const url::Origin& requesting_origin,
                                  const url::Origin& embedding_origin) override;

  void BlockBluetoothScanning(content::BrowserContext* browser_context,
                              const url::Origin& requesting_origin,
                              const url::Origin& embedding_origin) override;

  void GetMediaDeviceIDSalt(
      content::RenderFrameHost* rfh,
      const net::SiteForCookies& site_for_cookies,
      const blink::StorageKey& storage_key,
      base::OnceCallback<void(bool, const std::string&)> callback) override;

#if !BUILDFLAG(IS_ANDROID)
  base::OnceClosure FetchRemoteSms(
      content::WebContents* web_contents,
      const std::vector<url::Origin>& origin_list,
      base::OnceCallback<void(std::optional<std::vector<url::Origin>>,
                              std::optional<std::string>,
                              std::optional<content::SmsFetchFailureType>)>
          callback) override;
#endif

  bool IsClipboardPasteAllowed(
      content::RenderFrameHost* render_frame_host) override;

  void IsClipboardPasteAllowedByPolicy(
      const content::ClipboardEndpoint& source,
      const content::ClipboardEndpoint& destination,
      const content::ClipboardMetadata& metadata,
      ClipboardPasteData clipboard_paste_data,
      IsClipboardPasteAllowedCallback callback) override;

  void IsClipboardCopyAllowedByPolicy(
      const content::ClipboardEndpoint& source,
      const content::ClipboardMetadata& metadata,
      const ClipboardPasteData& data,
      IsClipboardCopyAllowedCallback callback) override;

#if BUILDFLAG(ENABLE_VR)
  content::XrIntegrationClient* GetXrIntegrationClient() override;
#endif

  void BindBrowserControlInterface(mojo::ScopedMessagePipeHandle pipe) override;
  bool ShouldInheritCrossOriginEmbedderPolicyImplicitly(
      const GURL& url) override;
  bool ShouldServiceWorkerInheritPolicyContainerFromCreator(
      const GURL& url) override;
  void GrantAdditionalRequestPrivilegesToWorkerProcess(
      int child_id,
      const GURL& script_url) override;
  PrivateNetworkRequestPolicyOverride ShouldOverridePrivateNetworkRequestPolicy(
      content::BrowserContext* browser_context,
      const url::Origin& origin) override;
  bool IsJitDisabledForSite(content::BrowserContext* browser_context,
                            const GURL& site_url) override;
  bool AreV8OptimizationsDisabledForSite(
      content::BrowserContext* browser_context,
      const GURL& site_url) override;
  ukm::UkmService* GetUkmService() override;

  blink::mojom::OriginTrialsSettingsPtr GetOriginTrialsSettings() override;

  void OnKeepaliveRequestStarted(
      content::BrowserContext* browser_context) override;
  void OnKeepaliveRequestFinished() override;

#if BUILDFLAG(IS_MAC)
  bool SetupEmbedderSandboxParameters(
      sandbox::mojom::Sandbox sandbox_type,
      sandbox::SandboxCompiler* compiler) override;
#endif  // BUILDFLAG(IS_MAC)

  void GetHyphenationDictionary(
      base::OnceCallback<void(const base::FilePath&)>) override;
  bool HasErrorPage(int http_status_code) override;

  StartupData* startup_data() { return &startup_data_; }

  std::unique_ptr<content::IdentityRequestDialogController>
  CreateIdentityRequestDialogController(
      content::WebContents* web_contents) override;

  std::unique_ptr<content::DigitalIdentityProvider>
  CreateDigitalIdentityProvider() override;

#if !BUILDFLAG(IS_ANDROID)
  base::TimeDelta GetKeepaliveTimerTimeout(content::BrowserContext* context);
#endif  // !BUILDFLAG(IS_ANDROID)

  bool SuppressDifferentOriginSubframeJSDialogs(
      content::BrowserContext* browser_context) override;

  std::unique_ptr<content::AnchorElementPreconnectDelegate>
  CreateAnchorElementPreconnectDelegate(
      content::RenderFrameHost& render_frame_host) override;

  std::unique_ptr<content::SpeculationHostDelegate>
  CreateSpeculationHostDelegate(
      content::RenderFrameHost& render_frame_host) override;

  std::unique_ptr<content::PrefetchServiceDelegate>
  CreatePrefetchServiceDelegate(
      content::BrowserContext* browser_context) override;

  std::unique_ptr<content::PrerenderWebContentsDelegate>
  CreatePrerenderWebContentsDelegate() override;

  void OnWebContentsCreated(content::WebContents* web_contents) override;

  bool IsFindInPageDisabledForOrigin(const url::Origin& origin) override;
  bool WillProvidePublicFirstPartySets() override;

  bool ShouldPreconnectNavigation(
      content::RenderFrameHost* render_frame_host) override;

  bool ShouldDisableOriginAgentClusterDefault(
      content::BrowserContext* browser_context) override;

  content::mojom::AlternativeErrorPageOverrideInfoPtr
  GetAlternativeErrorPageOverrideInfo(
      const GURL& url,
      content::RenderFrameHost* render_frame_host,
      content::BrowserContext* browser_context,
      int32_t error_code) override;

  void OnSharedStorageWorkletHostCreated(
      content::RenderFrameHost* rfh) override;

  void OnSharedStorageSelectURLCalled(
      content::RenderFrameHost* main_rfh) override;

  bool ShouldSendOutermostOriginToRenderer(
      const url::Origin& outermost_origin) override;

  bool IsFileSystemURLNavigationAllowed(
      content::BrowserContext* browser_context,
      const GURL& url) override;

  bool AreIsolatedWebAppsEnabled(
      content::BrowserContext* browser_context) override;

  bool IsThirdPartyStoragePartitioningAllowed(
      content::BrowserContext* browser_context,
      const url::Origin& top_level_origin) override;

  bool AreDeprecatedAutomaticBeaconCredentialsAllowed(
      content::BrowserContext* browser_context,
      const GURL& destination_url,
      const url::Origin& top_frame_origin) override;

  bool IsTransientActivationRequiredForShowFileOrDirectoryPicker(
      content::WebContents* web_contents) override;

  bool ShouldUseFirstPartyStorageKey(const url::Origin& origin) override;

  std::unique_ptr<content::ResponsivenessCalculatorDelegate>
  CreateResponsivenessCalculatorDelegate() override;

  bool CanBackForwardCachedPageReceiveCookieChanges(
      content::BrowserContext& browser_context,
      const GURL& url,
      const net::SiteForCookies& site_for_cookies,
      const std::optional<url::Origin>& top_frame_origin,
      const net::CookieSettingOverrides overrides) override;

  void GetCloudIdentifiers(
      const storage::FileSystemURL& url,
      content::FileSystemAccessPermissionContext::HandleType handle_type,
      GetCloudIdentifiersCallback callback) override;

  bool ShouldAllowBackForwardCacheForCacheControlNoStorePage(
      content::BrowserContext* browser_context) override;

  void SetIsMinimalMode(bool minimal) override;

  bool UseOutermostMainFrameOrEmbedderForSubCaptureTargets() const override;

#if !BUILDFLAG(IS_ANDROID)
  void BindVideoEffectsManager(
      const std::string& device_id,
      content::BrowserContext* browser_context,
      mojo::PendingReceiver<media::mojom::VideoEffectsManager>
          video_effects_manager) override;

  void BindVideoEffectsProcessor(
      const std::string& device_id,
      content::BrowserContext* browser_context,
      mojo::PendingReceiver<video_effects::mojom::VideoEffectsProcessor>
          video_effects_processor) override;
#endif  // !BUILDFLAG(IS_ANDROID)

  void PreferenceRankAudioDeviceInfos(
      content::BrowserContext* browser_context,
      blink::WebMediaDeviceInfoArray& infos) override;
  void PreferenceRankVideoDeviceInfos(
      content::BrowserContext* browser_context,
      blink::WebMediaDeviceInfoArray& infos) override;
  network::mojom::IpProtectionProxyBypassPolicy
  GetIpProtectionProxyBypassPolicy() override;
  void MaybePrewarmHttpDiskCache(
      content::BrowserContext& browser_context,
      const std::optional<url::Origin>& initiator_origin,
      const GURL& navigation_url) override;
#if BUILDFLAG(IS_CHROMEOS)
  void NotifyMultiCaptureStateChanged(
      content::GlobalRenderFrameHostId capturer_rfh_id,
      const std::string& label,
      MultiCaptureChanged state) override;
#endif  // BUILDFLAG(IS_CHROMEOS)

  std::unique_ptr<content::DipsDelegate> CreateDipsDelegate() override;

  bool ShouldSuppressAXLoadComplete(content::RenderFrameHost* rfh) override;

  void BindAIManager(
      content::BrowserContext* browser_context,
      std::variant<content::RenderFrameHost*, base::SupportsUserData*> host,
      mojo::PendingReceiver<blink::mojom::AIManager> receiver) override;

#if !BUILDFLAG(IS_ANDROID)
  void QueryInstalledWebAppsByManifestId(
      const GURL& frame_url,
      const GURL& manifest_id,
      content::BrowserContext* browser_context,
      base::OnceCallback<void(std::optional<blink::mojom::RelatedApplication>)>
          callback) override;
#endif  // !BUILDFLAG(IS_ANDROID)

  bool IsSaveableNavigation(
      content::NavigationHandle* navigation_handle) override;

#if BUILDFLAG(IS_WIN)
  void OnUiaProviderRequested(bool uia_provider_enabled) override;
#endif

  base::ReadOnlySharedMemoryRegion GetPerformanceScenarioRegionForProcess(
      content::RenderProcessHost* process_host) override;
  base::ReadOnlySharedMemoryRegion GetGlobalPerformanceScenarioRegion()
      override;

  void SetSamplingProfiler(
      std::unique_ptr<MainThreadStackSamplingProfiler> sampling_profiler);

 protected:
  static bool HandleWebUI(GURL* url, content::BrowserContext* browser_context);
  static bool HandleWebUIReverse(GURL* url,
                                 content::BrowserContext* browser_context);
  virtual const ui::NativeTheme* GetWebTheme() const;  // For testing.

  // Used by subclasses (e.g. implemented by downstream embedders) to add
  // their own extra part objects.
  // TODO: This should receive unique_ptr<ChromeContentBrowserClientParts>.
  void AddExtraPart(ChromeContentBrowserClientParts* part);

  // Exposed for tests to perform dependency injection.
  virtual std::unique_ptr<HttpAuthCoordinator> CreateHttpAuthCoordinator();

 private:
  friend class DisableWebRtcEncryptionFlagTest;
  friend class InProcessBrowserTest;

  FRIEND_TEST_ALL_PREFIXES(ChromeSiteIsolationPolicyTest,
                           IsolatedOriginsContainChromeOrigins);

  // Initializes `network_contexts_parent_directory_` and
  // `safe_browsing_service_` on the UI thread.
  void InitOnUIThread();

  // Copies disable WebRTC encryption switch depending on the channel.
  static void MaybeCopyDisableWebRtcEncryptionSwitch(
      base::CommandLine* to_command_line,
      const base::CommandLine& from_command_line,
      version_info::Channel channel);

  void FileSystemAccessed(
      const GURL& url,
      const std::vector<content::GlobalRenderFrameHostId>& render_frames,
      base::OnceCallback<void(bool)> callback,
      bool allow);

#if BUILDFLAG(ENABLE_GUEST_VIEW)
  void GuestPermissionRequestHelper(
      const GURL& url,
      const std::vector<content::GlobalRenderFrameHostId>& render_frames,
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

  // Returns an AsyncCheckTracker object used for holding URL checkers for async
  // Safe Browsing check. It may return nullptr if the WebContents is null,
  // the ui_manager is null, the real-time check is not enabled, or the loader
  // is for no-state prefetch or frame prerender.
  safe_browsing::AsyncCheckTracker* GetAsyncCheckTracker(
      const base::RepeatingCallback<content::WebContents*()>& wc_getter,
      bool is_enterprise_lookup_enabled,
      bool is_consumer_lookup_enabled,
      safe_browsing::hash_realtime_utils::HashRealTimeSelection
          hash_realtime_selection,
      content::FrameTreeNodeId frame_tree_node_id);

  // Try to upload an enterprise legacy tech event to the enterprise management
  // server for admins.
  void ReportLegacyTechEvent(
      content::RenderFrameHost* render_frame_host,
      const std::string& type,
      const GURL& url,
      const GURL& frame_url,
      const std::string& filename,
      uint64_t line,
      uint64_t column,
      std::optional<content::LegacyTechCookieIssueDetails> cookie_issue_details)
      override;

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  std::unique_ptr<blink::URLLoaderThrottle>
  MaybeCreateSafeBrowsingURLLoaderThrottle(
      const network::ResourceRequest& request,
      content::BrowserContext* browser_context,
      const base::RepeatingCallback<content::WebContents*()>& wc_getter,
      content::FrameTreeNodeId frame_tree_node_id,
      std::optional<int64_t> navigation_id,
      Profile* profile);
#endif

#if !BUILDFLAG(IS_ANDROID)
  void OnKeepaliveTimerFired(
      std::unique_ptr<ScopedKeepAlive> keep_alive_handle);
#endif

  // If `bound_network` != net::base::kInvalidNetworkHandle, this will make sure
  // tha the chain of URLLoaderFactories will end up with a network factory that
  // knows how to target `bound_network` (see
  // network.mojom.NetworkContextParams::bound_network documentation).
  // WARNING: This must be the last interceptor in the chain as the proxying
  // URLLoaderFactory installed by this needs to be the one actually sending
  // packets over the network (to effectively target `bound_network`).
  void MaybeProxyNetworkBoundRequest(
      content::BrowserContext* browser_context,
      net::handles::NetworkHandle bound_network,
      network::URLLoaderFactoryBuilder& factory_builder,
      network::mojom::URLLoaderFactoryOverridePtr* factory_override,
      const net::IsolationInfo& isolation_info);

  mojo::Remote<network::mojom::NetworkContext>&
  get_network_bound_network_context_for_testing() {
    return network_bound_network_context_;
  }

  net::handles::NetworkHandle
  get_target_network_for_network_bound_network_context_for_testing() const {
    return target_network_for_network_bound_network_context_;
  }

  // True if the Gaia origin should be isolated in a dedicated process.
  static bool DoesGaiaOriginRequireDedicatedProcess();

  // Vector of additional ChromeContentBrowserClientParts.
  // Parts are deleted in the reverse order they are added.
  std::vector<std::unique_ptr<ChromeContentBrowserClientParts>> extra_parts_;

  scoped_refptr<safe_browsing::SafeBrowsingService> safe_browsing_service_;
  scoped_refptr<safe_browsing::UrlCheckerDelegate>
      safe_browsing_url_checker_delegate_;

  StartupData startup_data_;

#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<ChromeSerialDelegate> serial_delegate_;
  std::unique_ptr<ChromeHidDelegate> hid_delegate_;
  std::unique_ptr<ChromeDirectSocketsDelegate> direct_sockets_delegate_;
  std::unique_ptr<ChromeWebAuthenticationDelegate> web_authentication_delegate_;
#endif
  std::unique_ptr<ChromeBluetoothDelegate> bluetooth_delegate_;
  std::unique_ptr<ChromeUsbDelegate> usb_delegate_;
  std::unique_ptr<ChromePrivateNetworkDeviceDelegate>
      private_network_device_delegate_;

#if BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<content::SmartCardDelegate> smart_card_delegate_;
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_VR)
  std::unique_ptr<vr::ChromeXrIntegrationClient> xr_integration_client_;
#endif

  // Returned from GetNetworkContextsParentDirectory() but created on the UI
  // thread because it needs to access the Local State prefs.
  std::vector<base::FilePath> network_contexts_parent_directory_;

  // Handles web-browser implementation of the http auth feature.
  std::unique_ptr<HttpAuthCoordinator> http_auth_coordinator_;

#if !BUILDFLAG(IS_ANDROID)
  uint64_t num_keepalive_requests_ = 0;
  base::OneShotTimer keepalive_timer_;
  base::TimeTicks keepalive_deadline_;
#endif

#if BUILDFLAG(IS_MAC)
  std::string GetChildProcessSuffix(int child_flags) override;
#endif  // BUILDFLAG(IS_MAC)

  // NetworkContext used by WebContents targeting a network. Currently, due to
  // the intended use-case (captive portal login over CCT), we support only a
  // single additional NetworkContext: this simplifies the lifetime handling by
  // *a lot*.
  // Whenever a WebContent targeting a new network issues a load request, the
  // previous NetworkContext will be destroyed and a new one will be created.
  // This can lead to "trashing" when multiple WebContents, targeting different
  // networks, are loading resources simultaneuously, as they will keep
  // destroying each other's NetworkContext. This is a known limitation of the
  // current design, but, as mentioned above, this should not happened in the
  // intended use-case (there is no expectation that a user will connect to
  // multiple captive portals at once).
  mojo::Remote<network::mojom::NetworkContext> network_bound_network_context_;

  // Network to which `network_bound_network_context_` is bound to. If ==
  // net::handles::kInvalidNetworkHandle, `network_bound_network_context_` is
  // currently in a pending remote state (i.e., no underlying NetworkContext
  // exists).
  net::handles::NetworkHandle
      target_network_for_network_bound_network_context_ =
          net::handles::kInvalidNetworkHandle;

  // Tracks whether the browser was started in "minimal" mode (as opposed to
  // full browser mode), where most subsystems are not initialized.
  bool is_minimal_mode_ = false;

  std::unique_ptr<MainThreadStackSamplingProfiler> sampling_profiler_;

#if BUILDFLAG(IS_WIN)
  bool handled_uia_provider_request_ = false;
#endif

  base::WeakPtrFactory<ChromeContentBrowserClient> weak_factory_{this};
};

// DO NOT USE. Functions in this namespace are only for the migration of DIPS to
// the content/ folder. They will be deleted soon.
//
// TODO: crbug.com/369813097 - Remove this after DIPS migrates to //content.
namespace dips_move {
void GrantCookieAccessDueToHeuristic(content::BrowserContext* browser_context,
                                     const net::SchemefulSite& top_frame_site,
                                     const net::SchemefulSite& accessing_site,
                                     base::TimeDelta ttl,
                                     bool ignore_schemes);
bool IsFullCookieAccessAllowed(content::BrowserContext* browser_context,
                               content::WebContents* web_contents,
                               const GURL& url,
                               const blink::StorageKey& storage_key);
}  // namespace dips_move

#endif  // CHROME_BROWSER_CHROME_CONTENT_BROWSER_CLIENT_H_
