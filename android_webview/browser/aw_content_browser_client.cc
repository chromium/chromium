// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_content_browser_client.h"

#include <string>
#include <utility>
#include <vector>

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_browser_main_parts.h"
#include "android_webview/browser/aw_contents.h"
#include "android_webview/browser/aw_contents_client_bridge.h"
#include "android_webview/browser/aw_contents_io_thread_client.h"
#include "android_webview/browser/aw_cookie_access_policy.h"
#include "android_webview/browser/aw_devtools_manager_delegate.h"
#include "android_webview/browser/aw_login_delegate.h"
#include "android_webview/browser/aw_printing_message_filter.h"
#include "android_webview/browser/aw_proxying_url_loader_factory.h"
#include "android_webview/browser/aw_quota_permission_context.h"
#include "android_webview/browser/aw_settings.h"
#include "android_webview/browser/aw_url_checker_delegate_impl.h"
#include "android_webview/browser/aw_web_contents_view_delegate.h"
#include "android_webview/browser/net/aw_url_request_context_getter.h"
#include "android_webview/browser/renderer_host/aw_resource_dispatcher_host_delegate.h"
#include "android_webview/browser/tracing/aw_tracing_delegate.h"
#include "android_webview/common/aw_descriptors.h"
#include "android_webview/common/aw_switches.h"
#include "android_webview/common/crash_reporter/aw_crash_reporter_client.h"
#include "android_webview/common/render_view_messages.h"
#include "android_webview/common/url_constants.h"
#include "android_webview/grit/aw_resources.h"
#include "base/android/locale_utils.h"
#include "base/base_paths_android.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/scoped_file.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/cdm/browser/cdm_message_filter_android.h"
#include "components/crash/content/browser/child_exit_observer_android.h"
#include "components/navigation_interception/intercept_navigation_delegate.h"
#include "components/policy/content/policy_blacklist_navigation_throttle.h"
#include "components/policy/core/browser/browser_policy_connector_base.h"
#include "components/safe_browsing/browser/browser_url_loader_throttle.h"
#include "components/safe_browsing/browser/mojo_safe_browsing_impl.h"
#include "components/safe_browsing/features.h"
#include "components/services/heap_profiling/public/mojom/constants.mojom.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_loader_throttle.h"
#include "content/public/common/web_preferences.h"
#include "net/android/network_library.h"
#include "net/log/net_log.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_info.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "storage/browser/quota/quota_settings.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_bundle_android.h"
#include "ui/resources/grit/ui_resources.h"

#if BUILDFLAG(ENABLE_SPELLCHECK)
#include "components/spellcheck/browser/spell_check_host_impl.h"
#include "components/spellcheck/common/spellcheck_switches.h"
#endif

using content::BrowserThread;
using content::ResourceType;
using content::WebContents;

namespace android_webview {
namespace {
static bool g_should_create_task_scheduler = true;

// TODO(sgurun) move this to its own file.
// This class filters out incoming aw_contents related IPC messages for the
// renderer process on the IPC thread.
class AwContentsMessageFilter : public content::BrowserMessageFilter {
 public:
  explicit AwContentsMessageFilter(int process_id);

  // BrowserMessageFilter methods.
  void OverrideThreadForMessage(const IPC::Message& message,
                                BrowserThread::ID* thread) override;
  bool OnMessageReceived(const IPC::Message& message) override;

  void OnShouldOverrideUrlLoading(int routing_id,
                                  const base::string16& url,
                                  bool has_user_gesture,
                                  bool is_redirect,
                                  bool is_main_frame,
                                  bool* ignore_navigation);
  void OnSubFrameCreated(int parent_render_frame_id, int child_render_frame_id);

 private:
  ~AwContentsMessageFilter() override;

  int process_id_;

  DISALLOW_COPY_AND_ASSIGN(AwContentsMessageFilter);
};

AwContentsMessageFilter::AwContentsMessageFilter(int process_id)
    : BrowserMessageFilter(AndroidWebViewMsgStart),
      process_id_(process_id) {
}

AwContentsMessageFilter::~AwContentsMessageFilter() {
}

void AwContentsMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message,
    BrowserThread::ID* thread) {
  if (message.type() == AwViewHostMsg_ShouldOverrideUrlLoading::ID) {
    *thread = BrowserThread::UI;
  }
}

bool AwContentsMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AwContentsMessageFilter, message)
    IPC_MESSAGE_HANDLER(AwViewHostMsg_ShouldOverrideUrlLoading,
                        OnShouldOverrideUrlLoading)
    IPC_MESSAGE_HANDLER(AwViewHostMsg_SubFrameCreated, OnSubFrameCreated)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void AwContentsMessageFilter::OnShouldOverrideUrlLoading(
    int render_frame_id,
    const base::string16& url,
    bool has_user_gesture,
    bool is_redirect,
    bool is_main_frame,
    bool* ignore_navigation) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  *ignore_navigation = false;
  AwContentsClientBridge* client =
      AwContentsClientBridge::FromID(process_id_, render_frame_id);
  if (client) {
    if (!client->ShouldOverrideUrlLoading(url, has_user_gesture, is_redirect,
                                          is_main_frame, ignore_navigation)) {
      // If the shouldOverrideUrlLoading call caused a java exception we should
      // always return immediately here!
      return;
    }
  } else {
    LOG(WARNING) << "Failed to find the associated render view host for url: "
                 << url;
  }
}

void AwContentsMessageFilter::OnSubFrameCreated(int parent_render_frame_id,
                                                int child_render_frame_id) {
  AwContentsIoThreadClient::SubFrameCreated(
      process_id_, parent_render_frame_id, child_render_frame_id);
}

// A dummy binder for mojo interface autofill::mojom::PasswordManagerDriver.
void DummyBindPasswordManagerDriver(
    autofill::mojom::PasswordManagerDriverRequest request,
    content::RenderFrameHost* render_frame_host) {}

}  // anonymous namespace

// TODO(yirui): can use similar logic as in PrependToAcceptLanguagesIfNecessary
// in chrome/browser/android/preferences/pref_service_bridge.cc
// static
std::string AwContentBrowserClient::GetAcceptLangsImpl() {
  // Start with the current locale(s) in BCP47 format.
  std::string locales_string = AwContents::GetLocaleList();

  // If accept languages do not contain en-US, add in en-US which will be
  // used with a lower q-value.
  if (locales_string.find("en-US") == std::string::npos)
    locales_string += ",en-US";
  return locales_string;
}

// static
AwBrowserContext* AwContentBrowserClient::GetAwBrowserContext() {
  return AwBrowserContext::GetDefault();
}

AwContentBrowserClient::AwContentBrowserClient() : net_log_(new net::NetLog()) {
  // Although WebView does not support password manager feature, renderer code
  // could still request this interface, so we register a dummy binder which
  // just drops the incoming request, to avoid the 'Failed to locate a binder
  // for interface' error log..
  frame_interfaces_.AddInterface(
      base::BindRepeating(&DummyBindPasswordManagerDriver));
  sniff_file_urls_ = AwSettings::GetAllowSniffingFileUrls();
}

AwContentBrowserClient::~AwContentBrowserClient() {}

AwBrowserContext* AwContentBrowserClient::InitBrowserContext(
    std::unique_ptr<PrefService> pref_service,
    std::unique_ptr<policy::BrowserPolicyConnectorBase> policy_connector) {
  base::FilePath user_data_dir;
  if (!base::PathService::Get(base::DIR_ANDROID_APP_DATA, &user_data_dir)) {
    NOTREACHED() << "Failed to get app data directory for Android WebView";
  }
  browser_context_ = std::make_unique<AwBrowserContext>(
      user_data_dir, std::move(pref_service), std::move(policy_connector));
  return browser_context_.get();
}

content::BrowserMainParts* AwContentBrowserClient::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters) {
  return new AwBrowserMainParts(this);
}

content::WebContentsViewDelegate*
AwContentBrowserClient::GetWebContentsViewDelegate(
    content::WebContents* web_contents) {
  return AwWebContentsViewDelegate::Create(web_contents);
}

void AwContentBrowserClient::RenderProcessWillLaunch(
    content::RenderProcessHost* host,
    service_manager::mojom::ServiceRequest* service_request) {
  // Grant content: scheme access to the whole renderer process, since we impose
  // per-view access checks, and access is granted by default (see
  // AwSettings.mAllowContentUrlAccess).
  content::ChildProcessSecurityPolicy::GetInstance()->GrantRequestScheme(
      host->GetID(), url::kContentScheme);

  host->AddFilter(new AwContentsMessageFilter(host->GetID()));
  // WebView always allows persisting data.
  host->AddFilter(new cdm::CdmMessageFilterAndroid(true, false));
  host->AddFilter(new AwPrintingMessageFilter(host->GetID()));
}

bool AwContentBrowserClient::ShouldUseMobileFlingCurve() const {
  return true;
}

bool AwContentBrowserClient::IsHandledURL(const GURL& url) {
  if (!url.is_valid()) {
    // We handle error cases.
    return true;
  }

  const std::string scheme = url.scheme();
  DCHECK_EQ(scheme, base::ToLowerASCII(scheme));
  // See CreateJobFactory in aw_url_request_context_getter.cc for the
  // list of protocols that are handled.
  // TODO(mnaganov): Make this automatic.
  static const char* const kProtocolList[] = {
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
  for (size_t i = 0; i < base::size(kProtocolList); ++i) {
    if (scheme == kProtocolList[i])
      return true;
  }
  return net::URLRequest::IsHandledProtocol(scheme);
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
    // Pass crash reporter enabled state to renderer processes.
    if (crash_reporter::IsCrashReporterEnabled()) {
      command_line->AppendSwitch(::switches::kEnableCrashReporter);
    }
  }
}

std::string AwContentBrowserClient::GetApplicationLocale() {
  return base::android::GetDefaultLocaleString();
}

std::string AwContentBrowserClient::GetAcceptLangs(
    content::BrowserContext* context) {
  return GetAcceptLangsImpl();
}

const gfx::ImageSkia* AwContentBrowserClient::GetDefaultFavicon() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  // TODO(boliu): Bundle our own default favicon?
  return rb.GetImageSkiaNamed(IDR_DEFAULT_FAVICON);
}

bool AwContentBrowserClient::AllowAppCache(const GURL& manifest_url,
                           const GURL& first_party,
                           content::ResourceContext* context) {
  // WebView doesn't have a per-site policy for locally stored data,
  // instead AppCache can be disabled for individual WebViews.
  return true;
}


bool AwContentBrowserClient::AllowGetCookie(const GURL& url,
                                            const GURL& first_party,
                                            const net::CookieList& cookie_list,
                                            content::ResourceContext* context,
                                            int render_process_id,
                                            int render_frame_id) {
  return AwCookieAccessPolicy::GetInstance()->AllowGetCookie(url,
                                                             first_party,
                                                             cookie_list,
                                                             context,
                                                             render_process_id,
                                                             render_frame_id);
}

bool AwContentBrowserClient::AllowSetCookie(const GURL& url,
                                            const GURL& first_party,
                                            const net::CanonicalCookie& cookie,
                                            content::ResourceContext* context,
                                            int render_process_id,
                                            int render_frame_id) {
  return AwCookieAccessPolicy::GetInstance()->AllowSetCookie(
      url, first_party, cookie, context, render_process_id, render_frame_id);
}

void AwContentBrowserClient::AllowWorkerFileSystem(
    const GURL& url,
    content::ResourceContext* context,
    const std::vector<content::GlobalFrameRoutingId>& render_frames,
    base::Callback<void(bool)> callback) {
  callback.Run(true);
}

bool AwContentBrowserClient::AllowWorkerIndexedDB(
    const GURL& url,
    const base::string16& name,
    content::ResourceContext* context,
    const std::vector<content::GlobalFrameRoutingId>& render_frames) {
  return true;
}

content::QuotaPermissionContext*
AwContentBrowserClient::CreateQuotaPermissionContext() {
  return new AwQuotaPermissionContext;
}

void AwContentBrowserClient::GetQuotaSettings(
    content::BrowserContext* context,
    content::StoragePartition* partition,
    storage::OptionalQuotaSettingsCallback callback) {
  storage::GetNominalDynamicSettings(
      partition->GetPath(), context->IsOffTheRecord(), std::move(callback));
}

content::GeneratedCodeCacheSettings
AwContentBrowserClient::GetGeneratedCodeCacheSettings(
    content::BrowserContext* context) {
  // If we pass 0 for size, disk_cache will pick a default size using the
  // heuristics based on available disk size. These are implemented in
  // disk_cache::PreferredCacheSize in net/disk_cache/cache_util.cc.
  return content::GeneratedCodeCacheSettings(true, 0,
                                             AwBrowserContext::GetCacheDir());
}

void AwContentBrowserClient::AllowCertificateError(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    ResourceType resource_type,
    bool strict_enforcement,
    bool expired_previous_decision,
    const base::Callback<void(content::CertificateRequestResultType)>&
        callback) {
  AwContentsClientBridge* client =
      AwContentsClientBridge::FromWebContents(web_contents);
  bool cancel_request = true;
  if (client)
    client->AllowCertificateError(cert_error,
                                  ssl_info.cert.get(),
                                  request_url,
                                  callback,
                                  &cancel_request);
  if (cancel_request)
    callback.Run(content::CERTIFICATE_REQUEST_RESULT_TYPE_DENY);
}

void AwContentBrowserClient::SelectClientCertificate(
    content::WebContents* web_contents,
    net::SSLCertRequestInfo* cert_request_info,
    net::ClientCertIdentityList client_certs,
    std::unique_ptr<content::ClientCertificateDelegate> delegate) {
  AwContentsClientBridge* client =
      AwContentsClientBridge::FromWebContents(web_contents);
  if (client)
    client->SelectClientCertificate(cert_request_info, std::move(delegate));
}

bool AwContentBrowserClient::CanCreateWindow(
    content::RenderFrameHost* opener,
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

void AwContentBrowserClient::ResourceDispatcherHostCreated() {
  AwResourceDispatcherHostDelegate::ResourceDispatcherHostCreated();
}

net::NetLog* AwContentBrowserClient::GetNetLog() {
  return net_log_.get();
}

base::FilePath AwContentBrowserClient::GetDefaultDownloadDirectory() {
  // Android WebView does not currently use the Chromium downloads system.
  // Download requests are cancelled immedately when recognized; see
  // AwResourceDispatcherHost::CreateResourceHandlerForDownload. However the
  // download system still tries to start up and calls this before recognizing
  // the request has been cancelled.
  return base::FilePath();
}

std::string AwContentBrowserClient::GetDefaultDownloadName() {
  NOTREACHED() << "Android WebView does not use chromium downloads";
  return std::string();
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
  return false;
}

bool AwContentBrowserClient::IsPepperVpnProviderAPIAllowed(
    content::BrowserContext* browser_context,
    const GURL& url) {
  NOTREACHED() << "Android WebView does not support plugins";
  return false;
}

content::TracingDelegate* AwContentBrowserClient::GetTracingDelegate() {
  return new AwTracingDelegate();
}

void AwContentBrowserClient::GetAdditionalMappedFilesForChildProcess(
    const base::CommandLine& command_line,
    int child_process_id,
    content::PosixFileDescriptorInfo* mappings) {
  base::MemoryMappedFile::Region region;
  int fd = ui::GetMainAndroidPackFd(&region);
  mappings->ShareWithRegion(kAndroidWebViewMainPakDescriptor, fd, region);

  fd = ui::GetCommonResourcesPackFd(&region);
  mappings->ShareWithRegion(kAndroidWebView100PercentPakDescriptor, fd, region);

  fd = ui::GetLocalePackFd(&region);
  mappings->ShareWithRegion(kAndroidWebViewLocalePakDescriptor, fd, region);

  ::crash_reporter::ChildExitObserver::GetInstance()
      ->BrowserChildProcessStarted(child_process_id, mappings);
}

void AwContentBrowserClient::OverrideWebkitPrefs(
    content::RenderViewHost* rvh,
    content::WebPreferences* web_prefs) {
  AwSettings* aw_settings = AwSettings::FromWebContents(
      content::WebContents::FromRenderViewHost(rvh));
  if (aw_settings) {
    aw_settings->PopulateWebPreferences(web_prefs);
  }
}

std::vector<std::unique_ptr<content::NavigationThrottle>>
AwContentBrowserClient::CreateThrottlesForNavigation(
    content::NavigationHandle* navigation_handle) {
  std::vector<std::unique_ptr<content::NavigationThrottle>> throttles;
  // We allow intercepting only navigations within main frames. This
  // is used to post onPageStarted. We handle shouldOverrideUrlLoading
  // via a sync IPC.
  if (navigation_handle->IsInMainFrame()) {
    throttles.push_back(
        navigation_interception::InterceptNavigationDelegate::CreateThrottleFor(
            navigation_handle));
    throttles.push_back(std::make_unique<PolicyBlacklistNavigationThrottle>(
        navigation_handle, browser_context_.get()));
  }
  return throttles;
}

content::DevToolsManagerDelegate*
AwContentBrowserClient::GetDevToolsManagerDelegate() {
  return new AwDevToolsManagerDelegate();
}

std::unique_ptr<base::Value> AwContentBrowserClient::GetServiceManifestOverlay(
    base::StringPiece name) {
  int id = -1;
  if (name == content::mojom::kBrowserServiceName)
    id = IDR_AW_BROWSER_MANIFEST_OVERLAY;
  else if (name == content::mojom::kRendererServiceName)
    id = IDR_AW_RENDERER_MANIFEST_OVERLAY;
  else if (name == content::mojom::kUtilityServiceName)
    id = IDR_AW_UTILITY_MANIFEST_OVERLAY;
  if (id == -1)
    return nullptr;

  base::StringPiece manifest_contents =
      ui::ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
          id, ui::ScaleFactor::SCALE_FACTOR_NONE);
  return base::JSONReader::Read(manifest_contents);
}

void AwContentBrowserClient::BindInterfaceRequestFromFrame(
    content::RenderFrameHost* render_frame_host,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  frame_interfaces_.TryBindInterface(interface_name, &interface_pipe,
                                     render_frame_host);
}

bool AwContentBrowserClient::BindAssociatedInterfaceRequestFromFrame(
    content::RenderFrameHost* render_frame_host,
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle* handle) {
  if (interface_name == autofill::mojom::AutofillDriver::Name_) {
    autofill::ContentAutofillDriverFactory::BindAutofillDriver(
        autofill::mojom::AutofillDriverAssociatedRequest(std::move(*handle)),
        render_frame_host);
    return true;
  }

  return false;
}

void AwContentBrowserClient::ExposeInterfacesToRenderer(
    service_manager::BinderRegistry* registry,
    blink::AssociatedInterfaceRegistry* associated_registry,
    content::RenderProcessHost* render_process_host) {
  if (base::FeatureList::IsEnabled(network::features::kNetworkService) ||
      base::FeatureList::IsEnabled(safe_browsing::kCheckByURLLoaderThrottle)) {
    content::ResourceContext* resource_context =
        render_process_host->GetBrowserContext()->GetResourceContext();
    registry->AddInterface(
        base::BindRepeating(
            &safe_browsing::MojoSafeBrowsingImpl::MaybeCreate,
            render_process_host->GetID(), resource_context,
            base::BindRepeating(
                &AwContentBrowserClient::GetSafeBrowsingUrlCheckerDelegate,
                base::Unretained(this))),
        base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}));
  }
#if BUILDFLAG(ENABLE_SPELLCHECK)
  registry->AddInterface(
      base::BindRepeating(&SpellCheckHostImpl::Create),
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::UI}));
#endif
}

std::vector<std::unique_ptr<content::URLLoaderThrottle>>
AwContentBrowserClient::CreateURLLoaderThrottles(
    const network::ResourceRequest& request,
    content::ResourceContext* resource_context,
    const base::RepeatingCallback<content::WebContents*()>& wc_getter,
    content::NavigationUIData* navigation_ui_data,
    int frame_tree_node_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  std::vector<std::unique_ptr<content::URLLoaderThrottle>> result;

  if (base::FeatureList::IsEnabled(network::features::kNetworkService) ||
      base::FeatureList::IsEnabled(safe_browsing::kCheckByURLLoaderThrottle)) {
    auto* delegate = GetSafeBrowsingUrlCheckerDelegate();
    if (delegate && !delegate->ShouldSkipRequestCheck(
                        resource_context, request.url, frame_tree_node_id,
                        -1 /* render_process_id */, -1 /* render_frame_id */,
                        request.originated_from_service_worker)) {
      auto safe_browsing_throttle =
          safe_browsing::BrowserURLLoaderThrottle::MaybeCreate(delegate,
                                                               wc_getter);
      if (safe_browsing_throttle)
        result.push_back(std::move(safe_browsing_throttle));
    }
  }

  return result;
}

safe_browsing::UrlCheckerDelegate*
AwContentBrowserClient::GetSafeBrowsingUrlCheckerDelegate() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!safe_browsing_url_checker_delegate_) {
    safe_browsing_url_checker_delegate_ = new AwUrlCheckerDelegateImpl(
        browser_context_->GetSafeBrowsingDBManager(),
        browser_context_->GetSafeBrowsingUIManager(),
        browser_context_->GetSafeBrowsingWhitelistManager());
  }

  return safe_browsing_url_checker_delegate_.get();
}

bool AwContentBrowserClient::ShouldOverrideUrlLoading(
    int frame_tree_node_id,
    bool browser_initiated,
    const GURL& gurl,
    const std::string& request_method,
    bool has_user_gesture,
    bool is_redirect,
    bool is_main_frame,
    ui::PageTransition transition,
    bool* ignore_navigation) {
  *ignore_navigation = false;

  // Only GETs can be overridden.
  if (request_method != "GET")
    return true;

  bool application_initiated =
      browser_initiated || transition & ui::PAGE_TRANSITION_FORWARD_BACK;

  // Don't offer application-initiated navigations unless it's a redirect.
  if (application_initiated && !is_redirect)
    return true;

  // For HTTP schemes, only top-level navigations can be overridden. Similarly,
  // WebView Classic lets app override only top level about:blank navigations.
  // So we filter out non-top about:blank navigations here.
  //
  // Note: about:blank navigations are not received in this path at the moment,
  // they use the old SYNC IPC path as they are not handled by network stack.
  // However, the old path should be removed in future.
  if (!is_main_frame &&
      (gurl.SchemeIs(url::kHttpScheme) || gurl.SchemeIs(url::kHttpsScheme) ||
       gurl.SchemeIs(url::kAboutScheme)))
    return true;

  WebContents* web_contents =
      WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  if (web_contents == nullptr)
    return true;
  AwContentsClientBridge* client_bridge =
      AwContentsClientBridge::FromWebContents(web_contents);
  if (client_bridge == nullptr)
    return true;

  base::string16 url = base::UTF8ToUTF16(gurl.possibly_invalid_spec());
  return client_bridge->ShouldOverrideUrlLoading(
      url, has_user_gesture, is_redirect, is_main_frame, ignore_navigation);
}

bool AwContentBrowserClient::ShouldCreateTaskScheduler() {
  return g_should_create_task_scheduler;
}

scoped_refptr<content::LoginDelegate>
AwContentBrowserClient::CreateLoginDelegate(
    net::AuthChallengeInfo* auth_info,
    content::ResourceRequestInfo::WebContentsGetter web_contents_getter,
    const content::GlobalRequestID& request_id,
    bool is_main_frame,
    const GURL& url,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    bool first_auth_attempt,
    LoginAuthRequiredCallback auth_required_callback) {
  return AwLoginDelegate::Create(auth_info, web_contents_getter,
                                 first_auth_attempt,
                                 std::move(auth_required_callback));
}

bool AwContentBrowserClient::HandleExternalProtocol(
    const GURL& url,
    content::ResourceRequestInfo::WebContentsGetter web_contents_getter,
    int child_id,
    content::NavigationUIData* navigation_data,
    bool is_main_frame,
    ui::PageTransition page_transition,
    bool has_user_gesture) {
  // The AwURLRequestJobFactory implementation should ensure this method never
  // gets called.
  NOTREACHED();
  return false;
}

void AwContentBrowserClient::RegisterOutOfProcessServices(
    OutOfProcessServiceMap* services) {
  (*services)[heap_profiling::mojom::kServiceName] =
      base::BindRepeating(&base::ASCIIToUTF16, "Heap Profiling Service");
}

bool AwContentBrowserClient::ShouldEnableStrictSiteIsolation() {
  // TODO(lukasza): When/if we eventually add OOPIF support for AW we should
  // consider running AW tests with and without site-per-process (and this might
  // require returning true below).  Adding OOPIF support for AW is tracked by
  // https://crbug.com/869494.
  return false;
}

bool AwContentBrowserClient::WillCreateURLLoaderFactory(
    content::BrowserContext* browser_context,
    content::RenderFrameHost* frame,
    bool is_navigation,
    const url::Origin& request_initiator,
    network::mojom::URLLoaderFactoryRequest* factory_request,
    bool* bypass_redirect_checks) {
  DCHECK(base::FeatureList::IsEnabled(network::features::kNetworkService));
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto proxied_request = std::move(*factory_request);
  network::mojom::URLLoaderFactoryPtrInfo target_factory_info;
  *factory_request = mojo::MakeRequest(&target_factory_info);
  int process_id = is_navigation ? 0 : frame->GetProcess()->GetID();

  // Android WebView has one non off-the-record browser context.
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&AwProxyingURLLoaderFactory::CreateProxy, process_id,
                     std::move(proxied_request), std::move(target_factory_info),
                     nullptr /* AwInterceptedRequestHandler */));
  return true;
}

// static
void AwContentBrowserClient::DisableCreatingTaskScheduler() {
  g_should_create_task_scheduler = false;
}

}  // namespace android_webview
