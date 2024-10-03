// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_content_browser_client.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_browser_context_store.h"
#include "android_webview/browser/aw_browser_main_parts.h"
#include "android_webview/browser/aw_browser_process.h"
#include "android_webview/browser/aw_client_hints_controller_delegate.h"
#include "android_webview/browser/aw_contents.h"
#include "android_webview/browser/aw_contents_client_bridge.h"
#include "android_webview/browser/aw_contents_io_thread_client.h"
#include "android_webview/browser/aw_cookie_access_policy.h"
#include "android_webview/browser/aw_devtools_manager_delegate.h"
#include "android_webview/browser/aw_enterprise_helper.h"
#include "android_webview/browser/aw_feature_list_creator.h"
#include "android_webview/browser/aw_http_auth_handler.h"
#include "android_webview/browser/aw_settings.h"
#include "android_webview/browser/aw_speech_recognition_manager_delegate.h"
#include "android_webview/browser/aw_web_contents_view_delegate.h"
#include "android_webview/browser/cookie_manager.h"
#include "android_webview/browser/network_service/aw_browser_context_io_thread_handle.h"
#include "android_webview/browser/network_service/aw_proxy_config_monitor.h"
#include "android_webview/browser/network_service/aw_proxying_restricted_cookie_manager.h"
#include "android_webview/browser/network_service/aw_proxying_url_loader_factory.h"
#include "android_webview/browser/network_service/aw_url_loader_throttle.h"
#include "android_webview/browser/safe_browsing/aw_safe_browsing_navigation_throttle.h"
#include "android_webview/browser/safe_browsing/aw_url_checker_delegate_impl.h"
#include "android_webview/browser/supervised_user/aw_supervised_user_throttle.h"
#include "android_webview/browser/supervised_user/aw_supervised_user_url_classifier.h"
#include "android_webview/browser/tracing/aw_tracing_delegate.h"
#include "android_webview/common/aw_content_client.h"
#include "android_webview/common/aw_descriptors.h"
#include "android_webview/common/aw_features.h"
#include "android_webview/common/aw_paths.h"
#include "android_webview/common/aw_switches.h"
#include "android_webview/common/url_constants.h"
#include "base/android/build_info.h"
#include "base/android/locale_utils.h"
#include "base/base_paths_android.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/trace_event/base_tracing.h"
#include "build/build_config.h"
#include "components/crash/content/browser/crash_handler_host_linux.h"
#include "components/embedder_support/origin_trials/origin_trials_settings_storage.h"
#include "components/embedder_support/switches.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/navigation_interception/intercept_navigation_delegate.h"
#include "components/page_load_metrics/browser/metrics_navigation_throttle.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/policy/content/policy_blocklist_navigation_throttle.h"
#include "components/policy/core/browser/browser_policy_connector_base.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/async_check_tracker.h"
#include "components/safe_browsing/content/browser/browser_url_loader_throttle.h"
#include "components/safe_browsing/content/browser/mojo_safe_browsing_impl.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/hashprefix_realtime/hash_realtime_utils.h"
#include "components/url_matcher/url_matcher.h"
#include "components/url_matcher/url_util.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/file_url_loader.h"
#include "content/public/browser/frame_type.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/shared_cors_origin_access_list.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_descriptors.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/user_agent.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/android/network_library.h"
#include "net/cookies/site_for_cookies.h"
#include "net/dns/public/secure_dns_mode.h"
#include "net/http/http_util.h"
#include "net/net_buildflags.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_info.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_factory_builder.h"
#include "services/network/public/mojom/cookie_manager.mojom-forward.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_bundle_android.h"
#include "ui/display/util/display_util.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/resources/grit/ui_resources.h"

using content::BrowserThread;
using content::FrameType;
using content::WebContents;
using safe_browsing::AsyncCheckTracker;
using safe_browsing::hash_realtime_utils::HashRealTimeSelection;
using AttributionReportingOsRegistrar =
    content::ContentBrowserClient::AttributionReportingOsRegistrar;

namespace android_webview {
namespace {
#if DCHECK_IS_ON()
// A boolean value to determine if the NetworkContext has been created yet. This
// exists only to check correctness: g_check_cleartext_permitted may only be set
// before the NetworkContext has been created (otherwise,
// g_check_cleartext_permitted won't have any effect).
bool g_created_network_context_params = false;
#endif

// On apps targeting API level O or later, check cleartext is enforced.
bool g_check_cleartext_permitted = false;

BASE_FEATURE(kWebViewOptimizeXrwNavigationFlow,
             "WebViewOptimizeXrwNavigationFlow",
             base::FEATURE_ENABLED_BY_DEFAULT);

// A throttle which checks if the XRW origin trial is enabled for this
// navigation, and forwards it to the proxying loader factory.
class XrwNavigationThrottle : public content::NavigationThrottle {
 public:
  explicit XrwNavigationThrottle(content::NavigationHandle* handle)
      : NavigationThrottle(handle) {}
  ~XrwNavigationThrottle() override {
    AwProxyingURLLoaderFactory::ClearXrwResultForNavigation(
        navigation_handle()->GetNavigationId());
  }

  ThrottleCheckResult WillStartRequest() override {
    auto* handle = navigation_handle();
    AwProxyingURLLoaderFactory::SetXrwResultForNavigation(
        handle->GetURL(),
        handle->IsInOutermostMainFrame()
            ? blink::mojom::ResourceType::kMainFrame
            : blink::mojom::ResourceType::kSubFrame,
        handle->GetFrameTreeNodeId(), handle->GetNavigationId());
    return content::NavigationThrottle::PROCEED;
  }

  const char* GetNameForLogging() override { return "XrwNavigationThrottle"; }
};

// Get async check tracker to make Safe Browsing v5 check asynchronous
base::WeakPtr<AsyncCheckTracker> GetAsyncCheckTracker(
    const base::RepeatingCallback<content::WebContents*()>& wc_getter,
    content::FrameTreeNodeId frame_tree_node_id) {
  if (!base::FeatureList::IsEnabled(
          safe_browsing::kSafeBrowsingAsyncRealTimeCheck)) {
    return nullptr;
  }
  content::WebContents* web_contents = wc_getter.Run();
  // Check whether current frame is a pre-rendered frame. WebView does not
  // support NoStatePrefetch, so we do not check for that.
  if (web_contents == nullptr ||
      web_contents->IsPrerenderedFrame(frame_tree_node_id)) {
    return nullptr;
  }

  // Setting should_sync_checker_check_allowlist to false since the allowlist
  // is not available on WebView.
  return AsyncCheckTracker::GetOrCreateForWebContents(
             web_contents,
             AwBrowserProcess::GetInstance()->GetSafeBrowsingUIManager(),
             /*should_sync_checker_check_allowlist=*/false)
      ->GetWeakPtr();
}

}  // anonymous namespace

std::string GetProduct() {
  return embedder_support::GetProductAndVersion();
}

std::string GetUserAgent() {
  // "Version/4.0" had been hardcoded in the legacy WebView.
  std::string product = "Version/4.0 " + GetProduct();
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseMobileUserAgent)) {
    product += " Mobile";
  }

  if (base::FeatureList::IsEnabled(
          features::kWebViewReduceUAAndroidVersionDeviceModel)) {
    return content::BuildUnifiedPlatformUAFromProductAndExtraOs(product,
                                                                "; wv");
  }

  return content::BuildUserAgentFromProductAndExtraOSInfo(
      product, "; wv", content::IncludeAndroidBuildNumber::Include);
}

// TODO(yirui): can use similar logic as in PrependToAcceptLanguagesIfNecessary
// in chrome/browser/android/preferences/pref_service_bridge.cc
// static
std::string AwContentBrowserClient::GetAcceptLangsImpl() {
  // Start with the current locale(s) in BCP47 format.
  std::string locales_string = AwContents::GetLocaleList();

  // If accept languages do not contain en-US, add in en-US which will be
  // used with a lower q-value.
  if (!base::Contains(locales_string, "en-US")) {
    locales_string += ",en-US";
  }
  return locales_string;
}

// static
void AwContentBrowserClient::set_check_cleartext_permitted(bool permitted) {
#if DCHECK_IS_ON()
  DCHECK(!g_created_network_context_params);
#endif
  g_check_cleartext_permitted = permitted;
}

// static
bool AwContentBrowserClient::get_check_cleartext_permitted() {
  return g_check_cleartext_permitted;
}

AwContentBrowserClient::AwContentBrowserClient(
    AwFeatureListCreator* aw_feature_list_creator)
    : sniff_file_urls_(AwSettings::GetAllowSniffingFileUrls()),
      aw_feature_list_creator_(aw_feature_list_creator) {
  // |aw_feature_list_creator| should not be null. The AwBrowserContext will
  // take the PrefService owned by the creator as the Local State instead
  // of loading the JSON file from disk.
  DCHECK(aw_feature_list_creator_);
}

AwContentBrowserClient::~AwContentBrowserClient() = default;

void AwContentBrowserClient::OnNetworkServiceCreated(
    network::mojom::NetworkService* network_service) {
  // TODO(crbug.com/40693524): If CertVerifierServiceFactory is moved to
  // a separate process, this will likely need to be set somewhere else instead
  // of here.
  content::GetCertVerifierServiceFactory()->SetUseChromeRootStore(
      false, base::DoNothing());

  network_service->SetUpHttpAuth(network::mojom::HttpAuthStaticParams::New());
  network_service->ConfigureHttpAuthPrefs(
      AwBrowserProcess::GetInstance()->CreateHttpAuthDynamicParams());

  if (base::FeatureList::IsEnabled(features::kWebViewAsyncDns)) {
    enterprise::GetEnterpriseState(
        base::BindOnce([](enterprise::EnterpriseState state) {
          switch (state) {
            case enterprise::EnterpriseState::kUnknown:
              // If we cannot be certain about the enterprise state, we should
              // not enable the AsyncDNS resolver, but fall back on the system
              // resolver.
            case enterprise::EnterpriseState::kEnterpriseOwned:
              // On enterprise owned devices, we should use the system resolver
              // to make sure that we respect any network settings implemented
              // by the device owner.
              return;
            case enterprise::EnterpriseState::kNotOwned:
              content::GetNetworkService()->ConfigureStubHostResolver(
                  /*insecure_dns_client_enabled=*/true,
                  net::SecureDnsMode::kAutomatic, net::DnsOverHttpsConfig(),
                  /*additional_dns_types_enabled=*/true);
              break;
          }
        }));
  }
}

void AwContentBrowserClient::ConfigureNetworkContextParams(
    content::BrowserContext* context,
    bool in_memory,
    const base::FilePath& relative_partition_path,
    network::mojom::NetworkContextParams* network_context_params,
    cert_verifier::mojom::CertVerifierCreationParams*
        cert_verifier_creation_params) {
  DCHECK(context);

  content::GetNetworkService()->ConfigureHttpAuthPrefs(
      AwBrowserProcess::GetInstance()->CreateHttpAuthDynamicParams());

  AwBrowserContext* aw_context = static_cast<AwBrowserContext*>(context);
  aw_context->ConfigureNetworkContextParams(in_memory, relative_partition_path,
                                            network_context_params,
                                            cert_verifier_creation_params);

  mojo::PendingRemote<network::mojom::CookieManager> cookie_manager_remote;
  network_context_params->cookie_manager =
      cookie_manager_remote.InitWithNewPipeAndPassReceiver();

#if DCHECK_IS_ON()
  g_created_network_context_params = true;
#endif

  // Pass the mojo::PendingRemote<network::mojom::CookieManager> to
  // android_webview::CookieManager, so it can implement its APIs with this mojo
  // CookieManager.
  aw_context->GetCookieManager()->SetMojoCookieManager(
      std::move(cookie_manager_remote));
}

AwBrowserContext* AwContentBrowserClient::InitBrowserContext() {
  return AwBrowserContextStore::GetOrCreateInstance()->GetDefault();
}

std::unique_ptr<content::BrowserMainParts>
AwContentBrowserClient::CreateBrowserMainParts(bool /* is_integration_test */) {
  return std::make_unique<AwBrowserMainParts>(this);
}

std::unique_ptr<content::WebContentsViewDelegate>
AwContentBrowserClient::GetWebContentsViewDelegate(
    content::WebContents* web_contents) {
  return std::make_unique<AwWebContentsViewDelegate>(web_contents);
}

void AwContentBrowserClient::RenderProcessWillLaunch(
    content::RenderProcessHost* host) {
  // Grant content: scheme access to the whole renderer process, since we impose
  // per-view access checks, and access is granted by default (see
  // AwSettings.mAllowContentUrlAccess).
  content::ChildProcessSecurityPolicy::GetInstance()->GrantRequestScheme(
      host->GetID(), url::kContentScheme);
}

bool AwContentBrowserClient::IsExplicitNavigation(
    ui::PageTransition transition) {
  return ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_TYPED);
}

bool AwContentBrowserClient::IsHandledURL(const GURL& url) {
  if (!url.is_valid()) {
    // We handle error cases.
    return true;
  }

  const std::string scheme = url.scheme();
  DCHECK_EQ(scheme, base::ToLowerASCII(scheme));
  static const char* const kProtocolList[] = {
    url::kHttpScheme,
    url::kHttpsScheme,
#if BUILDFLAG(ENABLE_WEBSOCKETS)
    url::kWsScheme,
    url::kWssScheme,
#endif  // BUILDFLAG(ENABLE_WEBSOCKETS)
    url::kDataScheme,
    url::kBlobScheme,
    url::kFileSystemScheme,
    content::kChromeUIScheme,
    url::kContentScheme,
  };
  if (scheme == url::kFileScheme) {
    // Return false for the "special" file URLs, so they can be loaded
    // even if access to file: scheme is not granted to the child process.
    return !IsAndroidSpecialFileUrl(url);
  }
  for (const char* supported_protocol : kProtocolList) {
    if (scheme == supported_protocol) {
      return true;
    }
  }
  return false;
}

bool AwContentBrowserClient::ForceSniffingFileUrlsForHtml() {
  return sniff_file_urls_;
}

void AwContentBrowserClient::AppendExtraCommandLineSwitches(
    base::CommandLine* command_line,
    int child_process_id) {
  if (!command_line->HasSwitch(switches::kSingleProcess)) {
    // The only kind of a child process WebView can have is renderer or utility.
    std::string process_type =
        command_line->GetSwitchValueASCII(switches::kProcessType);
    DCHECK(process_type == switches::kRendererProcess ||
           process_type == switches::kUtilityProcess)
        << process_type;

    static const char* const kSwitchNames[] = {
        ::switches::kEnableCrashReporter,
        ::switches::kEnableCrashReporterForTesting,
        embedder_support::kOriginTrialDisabledFeatures,
        embedder_support::kOriginTrialPublicKey,
    };

    command_line->CopySwitchesFrom(*base::CommandLine::ForCurrentProcess(),
                                   kSwitchNames);
  }
}

std::string AwContentBrowserClient::GetApplicationLocale() {
  return base::android::GetDefaultLocaleString();
}

std::string AwContentBrowserClient::GetAcceptLangs(
    content::BrowserContext* context) {
  return GetAcceptLangsImpl();
}

gfx::ImageSkia AwContentBrowserClient::GetDefaultFavicon() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  // TODO(boliu): Bundle our own default favicon?
  return rb.GetImageNamed(IDR_DEFAULT_FAVICON).AsImageSkia();
}

content::GeneratedCodeCacheSettings
AwContentBrowserClient::GetGeneratedCodeCacheSettings(
    content::BrowserContext* context) {
  // WebView limits the main HTTP cache to 20MB; we need to set a comparable
  // limit for the code cache since the source file needs to be in the HTTP
  // cache for the code cache entry to be used. There are two code caches that
  // both use this value, so we pass 10MB to keep the total disk usage to
  // roughly 2x what it was before the code cache was implemented.
  // TODO(crbug.com/41419561): webview should have smarter cache sizing logic.
  AwBrowserContext* browser_context = static_cast<AwBrowserContext*>(context);
  return content::GeneratedCodeCacheSettings(
      true, 10 * 1024 * 1024, browser_context->GetHttpCachePath());
}

void AwContentBrowserClient::AllowCertificateError(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    bool is_primary_main_frame_request,
    bool strict_enforcement,
    base::OnceCallback<void(content::CertificateRequestResultType)> callback) {
  AwContentsClientBridge* client =
      AwContentsClientBridge::FromWebContents(web_contents);
  bool cancel_request = true;
  // We only call the callback once but we must pass ownership to a function
  // that conditionally calls it.
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  if (client) {
    client->AllowCertificateError(cert_error, ssl_info.cert.get(), request_url,
                                  std::move(split_callback.first),
                                  &cancel_request);
  }
  if (cancel_request) {
    std::move(split_callback.second)
        .Run(content::CERTIFICATE_REQUEST_RESULT_TYPE_DENY);
  }
}

base::OnceClosure AwContentBrowserClient::SelectClientCertificate(
    content::BrowserContext* browser_context,
    int process_id,
    content::WebContents* web_contents,
    net::SSLCertRequestInfo* cert_request_info,
    net::ClientCertIdentityList client_certs,
    std::unique_ptr<content::ClientCertificateDelegate> delegate) {
  AwContentsClientBridge* client =
      web_contents ? AwContentsClientBridge::FromWebContents(web_contents)
                   : nullptr;
  if (client) {
    client->SelectClientCertificate(cert_request_info, std::move(delegate));
  }
  return base::OnceClosure();
}

bool AwContentBrowserClient::CanCreateWindow(
    content::RenderFrameHost* opener,
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
    bool* no_javascript_access) {
  // We unconditionally allow popup windows at this stage and will give
  // the embedder the opporunity to handle displaying of the popup in
  // WebContentsDelegate::AddContents (via the
  // AwContentsClient.onCreateWindow callback).
  // Note that if the embedder has blocked support for creating popup
  // windows through AwSettings, then we won't get to this point as
  // the popup creation will have been blocked at the WebKit level.
  if (no_javascript_access) {
    *no_javascript_access = false;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(opener);
  AwSettings* settings = AwSettings::FromWebContents(web_contents);

  return (settings && settings->GetJavaScriptCanOpenWindowsAutomatically()) ||
         user_gesture;
}

base::FilePath AwContentBrowserClient::GetDefaultDownloadDirectory() {
  // Android WebView does not currently use the Chromium downloads system.
  // Download requests are cancelled immediately when recognized. However the
  // download system still tries to start up and calls this before recognizing
  // the request has been cancelled.
  return base::FilePath();
}

std::string AwContentBrowserClient::GetDefaultDownloadName() {
  NOTREACHED() << "Android WebView does not use chromium downloads";
}

std::optional<base::FilePath>
AwContentBrowserClient::GetLocalTracesDirectory() {
  base::FilePath user_data_dir;
  if (!base::PathService::Get(android_webview::DIR_LOCAL_TRACES,
                              &user_data_dir)) {
    return std::nullopt;
  }
  DCHECK(!user_data_dir.empty());
  return user_data_dir;
}

void AwContentBrowserClient::DidCreatePpapiPlugin(
    content::BrowserPpapiHost* browser_host) {
  NOTREACHED() << "Android WebView does not support plugins";
}

bool AwContentBrowserClient::AllowPepperSocketAPI(
    content::BrowserContext* browser_context,
    const GURL& url,
    bool private_api,
    const content::SocketPermissionRequest* params) {
  NOTREACHED() << "Android WebView does not support plugins";
}

bool AwContentBrowserClient::IsPepperVpnProviderAPIAllowed(
    content::BrowserContext* browser_context,
    const GURL& url) {
  NOTREACHED() << "Android WebView does not support plugins";
}

std::unique_ptr<content::TracingDelegate>
AwContentBrowserClient::CreateTracingDelegate() {
  return std::make_unique<AwTracingDelegate>();
}

void AwContentBrowserClient::GetAdditionalMappedFilesForChildProcess(
    const base::CommandLine& command_line,
    int child_process_id,
    content::PosixFileDescriptorInfo* mappings) {
  base::MemoryMappedFile::Region region;
  int fd = ui::GetMainAndroidPackFd(&region);
  if (base::FeatureList::IsEnabled(features::kWebViewCheckPakFileDescriptors)) {
    CHECK_GE(fd, 0);
  }
  mappings->ShareWithRegion(kAndroidWebViewMainPakDescriptor, fd, region);

  fd = ui::GetCommonResourcesPackFd(&region);
  if (base::FeatureList::IsEnabled(features::kWebViewCheckPakFileDescriptors)) {
    CHECK_GE(fd, 0);
  }
  mappings->ShareWithRegion(kAndroidWebView100PercentPakDescriptor, fd, region);

  fd = ui::GetLocalePackFd(&region);
  if (base::FeatureList::IsEnabled(features::kWebViewCheckPakFileDescriptors)) {
    CHECK_GE(fd, 0);
  }
  mappings->ShareWithRegion(kAndroidWebViewLocalePakDescriptor, fd, region);

  int crash_signal_fd =
      crashpad::CrashHandlerHost::Get()->GetDeathSignalSocket();
  if (crash_signal_fd >= 0) {
    mappings->Share(kCrashDumpSignal, crash_signal_fd);
  }
}

void AwContentBrowserClient::OverrideWebkitPrefs(
    content::WebContents* web_contents,
    blink::web_pref::WebPreferences* web_prefs) {
  AwSettings* aw_settings = AwSettings::FromWebContents(web_contents);
  if (aw_settings) {
    aw_settings->PopulateWebPreferences(web_prefs);
  }
  web_prefs->modal_context_menu =
      !base::FeatureList::IsEnabled(features::kWebViewImageDrag);
}

std::vector<std::unique_ptr<content::NavigationThrottle>>
AwContentBrowserClient::CreateThrottlesForNavigation(
    content::NavigationHandle* navigation_handle) {
  std::vector<std::unique_ptr<content::NavigationThrottle>> throttles;
  // We allow intercepting only navigations within main frames. This
  // is used to post onPageStarted. We handle shouldOverrideUrlLoading
  // via a sync IPC.
  if (navigation_handle->IsInMainFrame()) {
    // MetricsNavigationThrottle requires that it runs before
    // NavigationThrottles that may delay or cancel navigations, so only
    // NavigationThrottles that don't delay or cancel navigations (e.g.
    // throttles that are only observing callbacks without affecting navigation
    // behavior) should be added before MetricsNavigationThrottle.
    throttles.push_back(page_load_metrics::MetricsNavigationThrottle::Create(
        navigation_handle));
  }
  // Use Synchronous mode for the navigation interceptor, since this class
  // doesn't actually call into an arbitrary client, it just posts a task to
  // call onPageStarted. shouldOverrideUrlLoading happens earlier (see
  // ContentBrowserClient::ShouldOverrideUrlLoading).
  std::unique_ptr<content::NavigationThrottle> intercept_navigation_throttle =
      navigation_interception::InterceptNavigationDelegate::
          MaybeCreateThrottleFor(navigation_handle,
                                 navigation_interception::SynchronyMode::kSync);
  if (intercept_navigation_throttle) {
    throttles.push_back(std::move(intercept_navigation_throttle));
  }

  throttles.push_back(std::make_unique<PolicyBlocklistNavigationThrottle>(
      navigation_handle,
      AwBrowserContext::FromWebContents(navigation_handle->GetWebContents())));

  std::unique_ptr<AwSafeBrowsingNavigationThrottle> safe_browsing_throttle =
      AwSafeBrowsingNavigationThrottle::MaybeCreateThrottleFor(
          navigation_handle);
  if (safe_browsing_throttle) {
    throttles.push_back(std::move(safe_browsing_throttle));
  }
  if (base::FeatureList::IsEnabled(kWebViewOptimizeXrwNavigationFlow)) {
    throttles.push_back(
        std::make_unique<XrwNavigationThrottle>(navigation_handle));
  }

  if ((navigation_handle->GetNavigatingFrameType() ==
           FrameType::kPrimaryMainFrame ||
       navigation_handle->GetNavigatingFrameType() == FrameType::kSubframe) &&
      navigation_handle->GetURL().SchemeIsHTTPOrHTTPS()) {
    AwSupervisedUserUrlClassifier* urlClassifier =
        AwSupervisedUserUrlClassifier::GetInstance();
    if (urlClassifier->ShouldCreateThrottle()) {
      throttles.push_back(std::make_unique<AwSupervisedUserThrottle>(
          navigation_handle, urlClassifier));
    }
  }

  return throttles;
}

std::unique_ptr<content::DevToolsManagerDelegate>
AwContentBrowserClient::CreateDevToolsManagerDelegate() {
  return std::make_unique<AwDevToolsManagerDelegate>();
}

std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
AwContentBrowserClient::CreateURLLoaderThrottles(
    const network::ResourceRequest& request,
    content::BrowserContext* browser_context,
    const base::RepeatingCallback<content::WebContents*()>& wc_getter,
    content::NavigationUIData* navigation_ui_data,
    content::FrameTreeNodeId frame_tree_node_id,
    std::optional<int64_t> navigation_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Set lookup mechanism based on feature flag
  HashRealTimeSelection hash_real_time_selection;
  base::WeakPtr<AsyncCheckTracker> async_check_tracker;
  if (base::FeatureList::IsEnabled(safe_browsing::kHashPrefixRealTimeLookups)) {
    hash_real_time_selection = HashRealTimeSelection::kDatabaseManager;
    async_check_tracker = GetAsyncCheckTracker(wc_getter, frame_tree_node_id);
  } else {
    hash_real_time_selection = HashRealTimeSelection::kNone;
    async_check_tracker = nullptr;
  }

  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> result;
  result.push_back(safe_browsing::BrowserURLLoaderThrottle::Create(
      base::BindRepeating(
          [](AwContentBrowserClient* client) {
            return client->GetSafeBrowsingUrlCheckerDelegate();
          },
          base::Unretained(this)),
      wc_getter, frame_tree_node_id, navigation_id,
      // TODO(crbug.com/40663467): rt_lookup_service is
      // used to perform real time URL check, which is gated by UKM opted-in.
      // Since AW currently doesn't support UKM, this feature is not enabled.
      /* rt_lookup_service */ nullptr,
      /* hash_realtime_service */ nullptr,
      /* hash_realtime_selection */
      hash_real_time_selection,
      /* async_check_tracker */ async_check_tracker));

  if (request.destination == network::mojom::RequestDestination::kDocument) {
    const bool is_load_url =
        request.transition_type & ui::PAGE_TRANSITION_FROM_API;
    const bool is_go_back_forward =
        request.transition_type & ui::PAGE_TRANSITION_FORWARD_BACK;
    const bool is_reload = ui::PageTransitionCoreTypeIs(
        static_cast<ui::PageTransition>(request.transition_type),
        ui::PAGE_TRANSITION_RELOAD);
    if (is_load_url || is_go_back_forward || is_reload) {
      result.push_back(std::make_unique<AwURLLoaderThrottle>(
          static_cast<AwBrowserContext*>(browser_context)));
    }
  }

  return result;
}

std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
AwContentBrowserClient::CreateURLLoaderThrottlesForKeepAlive(
    const network::ResourceRequest& request,
    content::BrowserContext* browser_context,
    const base::RepeatingCallback<content::WebContents*()>& wc_getter,
    content::FrameTreeNodeId frame_tree_node_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Set lookup mechanism based on feature flag
  HashRealTimeSelection hash_real_time_selection =
      (base::FeatureList::IsEnabled(safe_browsing::kHashPrefixRealTimeLookups))
          ? HashRealTimeSelection::kDatabaseManager
          : HashRealTimeSelection::kNone;

  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> result;

  result.push_back(safe_browsing::BrowserURLLoaderThrottle::Create(
      base::BindRepeating(
          [](AwContentBrowserClient* client) {
            return client->GetSafeBrowsingUrlCheckerDelegate();
          },
          base::Unretained(this)),
      wc_getter, frame_tree_node_id, /*navigation_id=*/std::nullopt,
      // TODO(crbug.com/40663467): rt_lookup_service is
      // used to perform real time URL check, which is gated by UKM opted-in.
      // Since AW currently doesn't support UKM, this feature is not enabled.
      /* rt_lookup_service */ nullptr,
      /* hash_realtime_service */ nullptr,
      /* hash_realtime_selection */
      hash_real_time_selection,
      /* async_check_tracker */ nullptr));

  return result;
}

scoped_refptr<safe_browsing::UrlCheckerDelegate>
AwContentBrowserClient::GetSafeBrowsingUrlCheckerDelegate() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!safe_browsing_url_checker_delegate_) {
    safe_browsing_url_checker_delegate_ = new AwUrlCheckerDelegateImpl(
        AwBrowserProcess::GetInstance()->GetSafeBrowsingDBManager(),
        AwBrowserProcess::GetInstance()->GetSafeBrowsingUIManager(),
        AwBrowserProcess::GetInstance()->GetSafeBrowsingAllowlistManager());
  }

  return safe_browsing_url_checker_delegate_;
}

bool AwContentBrowserClient::ShouldOverrideUrlLoading(
    content::FrameTreeNodeId frame_tree_node_id,
    bool browser_initiated,
    const GURL& gurl,
    const std::string& request_method,
    bool has_user_gesture,
    bool is_redirect,
    bool is_outermost_main_frame,
    bool is_prerendering,
    ui::PageTransition transition,
    bool* ignore_navigation) {
  *ignore_navigation = false;

  // Only GETs can be overridden.
  if (request_method != "GET") {
    return true;
  }

  bool application_initiated =
      browser_initiated || transition & ui::PAGE_TRANSITION_FORWARD_BACK;

  // Don't offer application-initiated navigations unless it's a redirect.
  if (application_initiated && !is_redirect) {
    return true;
  }

  // For HTTP schemes, only top-level navigations can be overridden. Similarly,
  // WebView Classic lets app override only top level about:blank navigations.
  // So we filter out non-top about:blank navigations here.
  //
  // Note: about:blank navigations are not received in this path at the moment,
  // they use the old SYNC IPC path as they are not handled by network stack.
  // However, the old path should be removed in future.
  if (!is_outermost_main_frame &&
      (gurl.SchemeIs(url::kHttpScheme) || gurl.SchemeIs(url::kHttpsScheme) ||
       gurl.SchemeIs(url::kAboutScheme))) {
    return true;
  }

  WebContents* web_contents =
      WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  if (web_contents == nullptr) {
    return true;
  }
  AwContentsClientBridge* client_bridge =
      AwContentsClientBridge::FromWebContents(web_contents);
  if (client_bridge == nullptr) {
    return true;
  }

  std::u16string url = base::UTF8ToUTF16(gurl.possibly_invalid_spec());

  AwSettings* aw_settings = AwSettings::FromWebContents(web_contents);
  if ((gurl.SchemeIs(url::kHttpScheme) || gurl.SchemeIs(url::kHttpsScheme)) &&
      aw_settings->enterprise_authentication_app_link_policy_enabled() &&
      android_webview::AwBrowserProcess::GetInstance()
          ->GetEnterpriseAuthenticationAppLinkManager()
          ->IsEnterpriseAuthenticationUrl(gurl)) {
    bool success = client_bridge->SendBrowseIntent(url);
    if (success) {
      *ignore_navigation = true;
      return true;
    }
  }

  net::HttpRequestHeaders request_headers;
  if (is_prerendering) {
    // We pass the `Sec-Purpose` header to tell the embedder that the navigation
    // is for prerendering, within the existing API surface.
    request_headers.SetHeader("Sec-Purpose", "prefetch;prerender");
  }

  return client_bridge->ShouldOverrideUrlLoading(
      url, has_user_gesture, is_redirect, is_outermost_main_frame,
      request_headers, ignore_navigation);
}

bool AwContentBrowserClient::ShouldAllowSameSiteRenderFrameHostChange(
    const content::RenderFrameHost& rfh) {
  if (!base::FeatureList::IsEnabled(features::kWebViewRenderDocument)) {
    return false;
  }
  content::RenderFrameHost* rfh_ptr =
      const_cast<content::RenderFrameHost*>(&rfh);
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh_ptr);
  AwSettings* aw_settings = AwSettings::FromWebContents(web_contents);
  // Don't allow same-site RFH swap on non-crashed frames if the initial page
  // scale is non-default. See the comment in `AwSettings` about this for more
  // details.
  return !aw_settings || !rfh_ptr->IsRenderFrameLive() ||
         !aw_settings->initial_page_scale_is_non_default();
}

std::unique_ptr<content::LoginDelegate>
AwContentBrowserClient::CreateLoginDelegate(
    const net::AuthChallengeInfo& auth_info,
    content::WebContents* web_contents,
    content::BrowserContext* browser_context,
    const content::GlobalRequestID& request_id,
    bool is_request_for_primary_main_frame,
    bool is_request_for_navigation,
    const GURL& url,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    bool first_auth_attempt,
    LoginAuthRequiredCallback auth_required_callback) {
  return std::make_unique<AwHttpAuthHandler>(auth_info, web_contents,
                                             first_auth_attempt,
                                             std::move(auth_required_callback));
}

bool AwContentBrowserClient::HandleExternalProtocol(
    const GURL& url,
    content::WebContents::Getter wc_getter,
    content::FrameTreeNodeId frame_tree_node_id,
    content::NavigationUIData* navigation_data,
    bool is_primary_main_frame,
    bool /* is_in_fenced_frame_tree */,
    network::mojom::WebSandboxFlags /*sandbox_flags*/,
    ui::PageTransition page_transition,
    bool has_user_gesture,
    const std::optional<url::Origin>& initiating_origin,
    content::RenderFrameHost* initiator_document,
    const net::IsolationInfo& isolation_info,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>* out_factory) {
  // Sandbox flags
  // =============
  //
  // Contrary to the chrome/ implementation, sandbox flags are ignored. Webview
  // by itself to not invoke external apps. However it let the embedding
  // app to intercept the request and decide what to do. We need to be careful
  // here not breaking applications, so the sandbox flags are ignored.

  mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver =
      out_factory->InitWithNewPipeAndPassReceiver();

  content::WebContents* web_contents = wc_getter.Run();
  scoped_refptr<AwBrowserContextIoThreadHandle> browser_context_handle =
      web_contents == nullptr
          ? nullptr
          : base::MakeRefCounted<AwBrowserContextIoThreadHandle>(
                static_cast<AwBrowserContext*>(
                    web_contents->GetBrowserContext()));

  // We don't need to care for |security_options| as the factories constructed
  // below are used only for navigation.
  // We also don't care about retrieving cookies in this case because these will
  // be schemes unrelated to the regular network stack so it doesn't make sense
  // to look for cookies. Providing a nullopt for the cookie manager lets
  // the AwProxyingURLLoaderFactory know to skip that work.
  if (content::BrowserThread::CurrentlyOn(content::BrowserThread::IO)) {
    // Manages its own lifetime.
    new android_webview::AwProxyingURLLoaderFactory(
        std::nullopt /* cookie_manager */, nullptr /* cookie_access_policy */,
        isolation_info, frame_tree_node_id, std::move(receiver),
        mojo::NullRemote(), true /* intercept_only */,
        std::nullopt /* security_options */,
        nullptr /* xrw_allowlist_matcher */, std::move(browser_context_handle),
        std::nullopt /* navigation_id */);
  } else {
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
               content::FrameTreeNodeId frame_tree_node_id,
               scoped_refptr<AwBrowserContextIoThreadHandle>
                   browser_context_handle,
               const net::IsolationInfo& isolation_info) {
              // Manages its own lifetime.
              new android_webview::AwProxyingURLLoaderFactory(
                  std::nullopt /* cookie_manager */,
                  nullptr /* cookie_access_policy */, isolation_info,
                  frame_tree_node_id, std::move(receiver), mojo::NullRemote(),
                  true /* intercept_only */,
                  std::nullopt /* security_options */,
                  nullptr /* xrw_allowlist_matcher */,
                  std::move(browser_context_handle),
                  std::nullopt /* navigation_id */);
            },
            std::move(receiver), frame_tree_node_id,
            std::move(browser_context_handle), isolation_info));
  }
  return false;
}

void AwContentBrowserClient::RegisterNonNetworkSubresourceURLLoaderFactories(
    int render_process_id,
    int render_frame_id,
    const std::optional<url::Origin>& request_initiator_origin,
    NonNetworkURLLoaderFactoryMap* factories) {
  WebContents* web_contents = content::WebContents::FromRenderFrameHost(
      content::RenderFrameHost::FromID(render_process_id, render_frame_id));
  AwSettings* aw_settings = AwSettings::FromWebContents(web_contents);

  if (aw_settings && aw_settings->GetAllowFileAccess()) {
    AwBrowserContext* aw_browser_context =
        AwBrowserContext::FromWebContents(web_contents);
    factories->emplace(
        url::kFileScheme,
        content::CreateFileURLLoaderFactory(
            aw_browser_context->GetPath(),
            aw_browser_context->GetSharedCorsOriginAccessList()));
  }
}

bool AwContentBrowserClient::ShouldAllowNoLongerUsedProcessToExit() {
  // TODO(crbug.com/40803531): Add Android WebView support for allowing a
  // renderer process to exit when only non-live RenderFrameHosts remain,
  // without consulting the app's OnRenderProcessGone crash handlers.
  return false;
}

bool AwContentBrowserClient::ShouldIsolateErrorPage(bool in_main_frame) {
  return false;
}

bool AwContentBrowserClient::ShouldEnableStrictSiteIsolation() {
  // TODO(lukasza): When/if we eventually add OOPIF support for AW we should
  // consider running AW tests with and without site-per-process (and this might
  // require returning true below).  Adding OOPIF support for AW is tracked by
  // https://crbug.com/806404.
  return false;
}

size_t AwContentBrowserClient::GetMaxRendererProcessCountOverride() {
  // TODO(crbug.com/40560171): These options can currently can only be turned by
  // by manually overriding command line switches because
  // `ShouldDisableSiteIsolation` returns true. Should coordinate if/when
  // enabling this in production.
  if (content::SiteIsolationPolicy::UseDedicatedProcessesForAllSites() ||
      content::SiteIsolationPolicy::AreIsolatedOriginsEnabled() ||
      content::SiteIsolationPolicy::IsStrictOriginIsolationEnabled()) {
    // Do not restrict the max renderer process count for these site isolation
    // modes. This allows OOPIFs to happen on android webview.
    return 0u;  // Use default.
  }
  return 1u;
}

bool AwContentBrowserClient::ShouldDisableSiteIsolation(
    content::SiteIsolationMode site_isolation_mode) {
  // Since AW does not yet support OOPIFs, we must return true here to disable
  // features that may trigger OOPIFs, such as origin isolation.
  //
  // Adding OOPIF support for AW is tracked by https://crbug.com/806404.
  return true;
}

bool AwContentBrowserClient::ShouldLockProcessToSite(
    content::BrowserContext* browser_context,
    const GURL& effective_url) {
  // TODO(lukasza): https://crbug.com/806404: Once Android WebView supports
  // OOPIFs, we should remove this ShouldLockProcess overload.  Till then,
  // returning false helps avoid accidentally applying citadel-style Site
  // Isolation enforcement to Android WebView (and causing incorrect renderer
  // kills).
  return false;
}

bool AwContentBrowserClient::ShouldEnforceNewCanCommitUrlChecks() {
  // TODO(https://crbug.com/326250356): Diagnose any remaining Android WebView
  // crashes from these new checks and then remove this function.
  return true;
}

void AwContentBrowserClient::WillCreateURLLoaderFactory(
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
    scoped_refptr<base::SequencedTaskRunner> navigation_response_task_runner) {
  TRACE_EVENT0("android_webview",
               "AwContentBrowserClient::WillCreateURLLoaderFactory");
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  mojo::PendingReceiver<network::mojom::URLLoaderFactory> proxied_receiver;
  mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory_remote;

  if (factory_override) {
    // We are interested in factories "inside" of CORS, so use
    // |factory_override|.
    *factory_override = network::mojom::URLLoaderFactoryOverride::New();
    proxied_receiver =
        (*factory_override)
            ->overriding_factory.InitWithNewPipeAndPassReceiver();
    (*factory_override)->overridden_factory_receiver =
        target_factory_remote.InitWithNewPipeAndPassReceiver();
    (*factory_override)->skip_cors_enabled_scheme_check = true;
  } else {
    // In this case, |factory_override| is not given. But all callers of
    // ContentBrowserClient::WillCreateURLLoaderFactory guarantee that
    // |factory_override| is null only when the security features on the network
    // service is no-op for requests coming to the URLLoaderFactory. Hence we
    // can use |factory_builder| here.
    std::tie(proxied_receiver, target_factory_remote) =
        factory_builder.Append();
  }
  scoped_refptr<AwBrowserContextIoThreadHandle> browser_context_handle =
      base::MakeRefCounted<AwBrowserContextIoThreadHandle>(
          static_cast<AwBrowserContext*>(browser_context));

  mojo::PendingRemote<network::mojom::CookieManager> cookie_manager;
  browser_context->GetDefaultStoragePartition()
      ->GetNetworkContext()
      ->GetCookieManager(cookie_manager.InitWithNewPipeAndPassReceiver());

  AwBrowserContext* aw_browser_context =
      static_cast<AwBrowserContext*>(browser_context);
  AwCookieAccessPolicy* cookie_access_policy =
      aw_browser_context->GetCookieManager()->cookie_access_policy();

  if (frame) {
    auto security_options =
        std::make_optional<AwProxyingURLLoaderFactory::SecurityOptions>();
    security_options->disable_web_security =
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kDisableWebSecurity);
    WebContents* web_contents = WebContents::FromRenderFrameHost(frame);
    const auto& preferences = web_contents->GetOrCreateWebPreferences();
    // See also //android_webview/docs/cors-and-webview-api.md to understand how
    // each settings affect CORS behaviors on file:// and content://.
    if (request_initiator.scheme() == url::kFileScheme) {
      security_options->disable_web_security |=
          preferences.allow_universal_access_from_file_urls;
      // Usual file:// to file:// requests are mapped to kNoCors if the setting
      // is set to true. Howover, file:///android_{asset|res}/ still uses kCors
      // and needs to permit it in the |security_options|.
      security_options->allow_cors_to_same_scheme =
          preferences.allow_file_access_from_file_urls;
    } else if (request_initiator.scheme() == url::kContentScheme) {
      security_options->allow_cors_to_same_scheme =
          preferences.allow_file_access_from_file_urls ||
          preferences.allow_universal_access_from_file_urls;
    }

    auto xrw_allowlist_matcher =
        AwSettings::FromWebContents(web_contents)->xrw_allowlist_matcher();

    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &AwProxyingURLLoaderFactory::CreateProxy, std::move(cookie_manager),
            cookie_access_policy, isolation_info, frame->GetFrameTreeNodeId(),
            std::move(proxied_receiver), std::move(target_factory_remote),
            security_options, std::move(xrw_allowlist_matcher),
            std::move(browser_context_handle), navigation_id));
  } else {
    // A service worker and worker subresources set nullptr to |frame|, and
    // work without seeing the AllowUniversalAccessFromFileURLs setting. So,
    // we don't pass a valid |security_options| here.
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &AwProxyingURLLoaderFactory::CreateProxy, std::move(cookie_manager),
            cookie_access_policy, isolation_info, content::FrameTreeNodeId(),
            std::move(proxied_receiver), std::move(target_factory_remote),
            std::nullopt /* security_options */,
            aw_browser_context->service_worker_xrw_allowlist_matcher(),
            std::move(browser_context_handle), navigation_id));
  }
}

uint32_t AwContentBrowserClient::GetWebSocketOptions(
    content::RenderFrameHost* frame) {
  uint32_t options = network::mojom::kWebSocketOptionNone;
  if (!frame) {
    return options;
  }
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(frame);
  AwContents* aw_contents = AwContents::FromWebContents(web_contents);
  AwBrowserContext* aw_context =
      AwBrowserContext::FromWebContents(web_contents);

  bool global_cookie_policy = aw_context->GetCookieManager()
                                  ->cookie_access_policy()
                                  ->GetShouldAcceptCookies();
  bool third_party_cookie_policy = aw_contents->AllowThirdPartyCookies();
  if (!global_cookie_policy) {
    options |= network::mojom::kWebSocketOptionBlockAllCookies;
  } else if (!third_party_cookie_policy) {
    options |= network::mojom::kWebSocketOptionBlockThirdPartyCookies;
  }
  return options;
}

bool AwContentBrowserClient::WillCreateRestrictedCookieManager(
    network::mojom::RestrictedCookieManagerRole role,
    content::BrowserContext* browser_context,
    const url::Origin& origin,
    const net::IsolationInfo& isolation_info,
    bool is_service_worker,
    int process_id,
    int routing_id,
    mojo::PendingReceiver<network::mojom::RestrictedCookieManager>* receiver) {
  mojo::PendingReceiver<network::mojom::RestrictedCookieManager> orig_receiver =
      std::move(*receiver);

  mojo::PendingRemote<network::mojom::RestrictedCookieManager>
      target_rcm_remote;
  *receiver = target_rcm_remote.InitWithNewPipeAndPassReceiver();

  AwBrowserContext* aw_context =
      static_cast<AwBrowserContext*>(browser_context);
  AwCookieAccessPolicy* aw_cookie_access_policy =
      aw_context->GetCookieManager()->cookie_access_policy();

  AwProxyingRestrictedCookieManager::CreateAndBind(
      std::move(target_rcm_remote), is_service_worker, process_id, routing_id,
      std::move(orig_receiver), aw_cookie_access_policy);

  return false;  // only made a proxy, still need the actual impl to be made.
}

std::string AwContentBrowserClient::GetProduct() {
  // Return the unreduced product version regardless of the user agent reduction
  // policy. The call sites do not require user agent reduction and having the
  // unreduced version is necessary for performance tracing.
  if (base::FeatureList::IsEnabled(features::kWebViewUnreducedProductVersion)) {
    return std::string(version_info::GetProductNameAndVersionForUserAgent());
  }
  return android_webview::GetProduct();
}

std::string AwContentBrowserClient::GetUserAgent() {
  return android_webview::GetUserAgent();
}

blink::UserAgentMetadata AwContentBrowserClient::GetUserAgentMetadata() {
  return AwClientHintsControllerDelegate::GetUserAgentMetadataOverrideBrand();
}

content::ContentBrowserClient::WideColorGamutHeuristic
AwContentBrowserClient::GetWideColorGamutHeuristic() {
  if (base::FeatureList::IsEnabled(features::kWebViewWideColorGamutSupport)) {
    return WideColorGamutHeuristic::kUseWindow;
  }

  if (display::HasForceDisplayColorProfile() &&
      display::GetForcedDisplayColorProfile() ==
          gfx::ColorSpace::CreateDisplayP3D65()) {
    return WideColorGamutHeuristic::kUseWindow;
  }

  return WideColorGamutHeuristic::kNone;
}

void AwContentBrowserClient::LogWebFeatureForCurrentPage(
    content::RenderFrameHost* render_frame_host,
    blink::mojom::WebFeature feature) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  page_load_metrics::MetricsWebContentsObserver::RecordFeatureUsage(
      render_frame_host, feature);
}

void AwContentBrowserClient::LogWebDXFeatureForCurrentPage(
    content::RenderFrameHost* render_frame_host,
    blink::mojom::WebDXFeature feature) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  page_load_metrics::MetricsWebContentsObserver::RecordFeatureUsage(
      render_frame_host, feature);
}

content::ContentBrowserClient::PrivateNetworkRequestPolicyOverride
AwContentBrowserClient::ShouldOverridePrivateNetworkRequestPolicy(
    content::BrowserContext* browser_context,
    const url::Origin& origin) {
  // Webview does not implement support for deprecation trials, so webview apps
  // broken by Private Network Access restrictions cannot help themselves by
  // registering for the trial.
  // See crbug.com/1255675.
  return content::ContentBrowserClient::PrivateNetworkRequestPolicyOverride::
      kForceAllow;
}

content::SpeechRecognitionManagerDelegate*
AwContentBrowserClient::CreateSpeechRecognitionManagerDelegate() {
  return new AwSpeechRecognitionManagerDelegate();
}

bool AwContentBrowserClient::HasErrorPage(int http_status_code) {
  return http_status_code >= 400;
}

bool AwContentBrowserClient::SuppressDifferentOriginSubframeJSDialogs(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(
      features::kWebViewSuppressDifferentOriginSubframeJSDialogs);
}

bool AwContentBrowserClient::ShouldPreconnectNavigation(
    content::RenderFrameHost* render_frame_host) {
  // This didn't make a performance improvement in WebView.
  return false;
}

void AwContentBrowserClient::OnDisplayInsecureContent(
    content::WebContents* web_contents) {
  AwSettings* aw_settings = AwSettings::FromWebContents(web_contents);
  if (aw_settings) {
    UMA_HISTOGRAM_ENUMERATION(
        "Android.WebView.OptionallyBlockableMixedContentLoaded.Mode",
        aw_settings->GetMixedContentMode(),
        AwSettings::MixedContentMode::COUNT);
  }
}

blink::mojom::OriginTrialsSettingsPtr
AwContentBrowserClient::GetOriginTrialsSettings() {
  return AwBrowserProcess::GetInstance()
      ->GetOriginTrialsSettingsStorage()
      ->GetSettings();
}

network::mojom::AttributionSupport
AwContentBrowserClient::GetAttributionSupport(
    AttributionReportingOsApiState state,
    bool client_os_disabled) {
  // WebView only supports OS-level attribution and not web-attribution.
  switch (state) {
    case AttributionReportingOsApiState::kDisabled:
      return network::mojom::AttributionSupport::kNone;
    case AttributionReportingOsApiState::kEnabled:
      return client_os_disabled ? network::mojom::AttributionSupport::kNone
                                : network::mojom::AttributionSupport::kOs;
  }
}

bool AwContentBrowserClient::IsAttributionReportingOperationAllowed(
    content::BrowserContext* browser_context,
    AttributionReportingOperation operation,
    content::RenderFrameHost* rfh,
    const url::Origin* source_origin,
    const url::Origin* destination_origin,
    const url::Origin* reporting_origin,
    bool* can_bypass) {
  AwBrowserContext* aw_context =
      static_cast<AwBrowserContext*>(browser_context);
  // WebView only supports OS-level attribution and not web-attribution.
  // Note: We do not check here if attribution reporting has been disabled
  // for the associated WebView as this is checked at the start of processing
  // an attribution event.
  switch (operation) {
    case AttributionReportingOperation::kAny:
    case AttributionReportingOperation::kOsSource:
    case AttributionReportingOperation::kOsTrigger:
    case AttributionReportingOperation::kOsSourceVerboseDebugReport:
    case AttributionReportingOperation::kOsTriggerVerboseDebugReport:
      return true;
    case AttributionReportingOperation::kSource:
    case AttributionReportingOperation::kTrigger:
    case AttributionReportingOperation::kSourceVerboseDebugReport:
    case AttributionReportingOperation::kTriggerVerboseDebugReport:
    case AttributionReportingOperation::kReport:
    case AttributionReportingOperation::kSourceTransitionalDebugReporting:
    case AttributionReportingOperation::kTriggerTransitionalDebugReporting:
    case AttributionReportingOperation::kSourceAggregatableDebugReport:
    case AttributionReportingOperation::kTriggerAggregatableDebugReport:
      return false;
    case AttributionReportingOperation::kOsSourceTransitionalDebugReporting:
    case AttributionReportingOperation::kOsTriggerTransitionalDebugReporting: {
      if (!aw_context->GetCookieManager()
               ->cookie_access_policy()
               ->GetShouldAcceptCookies()) {
        return false;
      }

      WebContents* web_contents =
          content::WebContents::FromRenderFrameHost(rfh);
      AwSettings* aw_settings = AwSettings::FromWebContents(web_contents);
      if (!aw_settings) {
        return false;
      }

      return aw_settings->GetAllowThirdPartyCookies();
    }
  }

  NOTREACHED();
}

content::ContentBrowserClient::AttributionReportingOsRegistrars
AwContentBrowserClient::GetAttributionReportingOsRegistrars(
    content::WebContents* web_contents) {
  // Attribution reporting can register a source to either the top level origin
  // or the app. For WebView the default is to register sources against the app
  // as:
  // 1. WebViews are often used in cases where for sources the top level origin
  // is not as relevant as the app context.
  // 2. Web registration APIs currently require a special registration from the
  // app in Android for registering sources and the more common case is that the
  // app does not have this registration. Note: This behaviour can be switched
  // to registering against the top level origin via an AndroidX API

  // Attribution reporting can register a trigger to either the top level origin
  // or the app. For WebView the default is to register triggers against the top
  // level origin as:
  // 1. WebViews are mostly used in cases where for triggers the app context is
  // not as relevant as the top level origin. Note: This behaviour can be
  // switched to registering against the app via an AndroidX API

  AwSettings* aw_settings = AwSettings::FromWebContents(web_contents);

  if (!aw_settings) {
    return {AttributionReportingOsRegistrar::kDisabled,
            AttributionReportingOsRegistrar::kDisabled};
  }

  AwSettings::AttributionBehavior attribution_behavior =
      aw_settings->GetAttributionBehavior();

  switch (attribution_behavior) {
    case AwSettings::AttributionBehavior::WEB_SOURCE_AND_WEB_TRIGGER:
      return {AttributionReportingOsRegistrar::kWeb,
              AttributionReportingOsRegistrar::kWeb};
    case AwSettings::AttributionBehavior::APP_SOURCE_AND_WEB_TRIGGER:
      return {AttributionReportingOsRegistrar::kOs,
              AttributionReportingOsRegistrar::kWeb};
    case AwSettings::AttributionBehavior::APP_SOURCE_AND_APP_TRIGGER:
      return {AttributionReportingOsRegistrar::kOs,
              AttributionReportingOsRegistrar::kOs};
    case AwSettings::AttributionBehavior::DISABLED:
      return {AttributionReportingOsRegistrar::kDisabled,
              AttributionReportingOsRegistrar::kDisabled};
  }

  NOTREACHED();
}

network::mojom::IpProtectionProxyBypassPolicy
AwContentBrowserClient::GetIpProtectionProxyBypassPolicy() {
  // The exact WebView-specific exclusion policy that is used will depend
  // on android_webview::features::kWebViewIpProtectionExclusionCriteria
  return network::mojom::IpProtectionProxyBypassPolicy::kExclusionList;
}

bool AwContentBrowserClient::WillProvidePublicFirstPartySets() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kWebViewFpsComponent);
}

bool AwContentBrowserClient::IsFullCookieAccessAllowed(
    content::BrowserContext* browser_context,
    content::WebContents* web_contents,
    const GURL& url,
    const blink::StorageKey& storage_key) {
  if (!web_contents) {
    // We do not allow third-party cookie access from service workers.
    return false;
  }
  AwSettings* aw_settings = AwSettings::FromWebContents(web_contents);
  if (!aw_settings) {
    return false;
  }

  return aw_settings->GetAllowThirdPartyCookies();
}

bool AwContentBrowserClient::AllowNonActivatedCrossOriginPaintHolding() {
  // In WebView, we allow non-activated cross-origin paint holding, since apps
  // currently experience this behavior and are in control of what to show.
  // TODO(crbug.com/368087192): We can consider disabling it while monitoring
  // for any breakages.
  return true;
}

}  // namespace android_webview
