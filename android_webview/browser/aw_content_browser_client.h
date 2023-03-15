// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_CONTENT_BROWSER_CLIENT_H_
#define ANDROID_WEBVIEW_BROWSER_AW_CONTENT_BROWSER_CLIENT_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "storage/browser/quota/quota_settings.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace safe_browsing {
class UrlCheckerDelegate;
}  // namespace safe_browsing

namespace net {
class IsolationInfo;
}  // namespace net

namespace android_webview {

class AwBrowserContext;
class AwFeatureListCreator;

std::string GetProduct();
std::string GetUserAgent();

class AwContentBrowserClient : public content::ContentBrowserClient {
 public:
  // This is what AwContentBrowserClient::GetAcceptLangs uses.
  static std::string GetAcceptLangsImpl();

  // Sets whether the net stack should check the cleartext policy from the
  // platform. For details, see
  // https://developer.android.com/reference/android/security/NetworkSecurityPolicy.html#isCleartextTrafficPermitted().
  static void set_check_cleartext_permitted(bool permitted);
  static bool get_check_cleartext_permitted();

  // |aw_feature_list_creator| should not be null.
  explicit AwContentBrowserClient(
      AwFeatureListCreator* aw_feature_list_creator);

  AwContentBrowserClient(const AwContentBrowserClient&) = delete;
  AwContentBrowserClient& operator=(const AwContentBrowserClient&) = delete;

  ~AwContentBrowserClient() override;

  // Allows AwBrowserMainParts to initialize a BrowserContext at the right
  // moment during startup. AwContentBrowserClient owns the result.
  AwBrowserContext* InitBrowserContext();

  // content::ContentBrowserClient:
  void OnNetworkServiceCreated(
      network::mojom::NetworkService* network_service) override;
  void ConfigureNetworkContextParams(
      content::BrowserContext* context,
      bool in_memory,
      const base::FilePath& relative_partition_path,
      network::mojom::NetworkContextParams* network_context_params,
      cert_verifier::mojom::CertVerifierCreationParams*
          cert_verifier_creation_params) override;
  std::unique_ptr<content::BrowserMainParts> CreateBrowserMainParts(
      bool is_integration_test) override;
  std::unique_ptr<content::WebContentsViewDelegate> GetWebContentsViewDelegate(
      content::WebContents* web_contents) override;
  void RenderProcessWillLaunch(content::RenderProcessHost* host) override;
  bool IsExplicitNavigation(ui::PageTransition transition) override;
  bool IsHandledURL(const GURL& url) override;
  bool ForceSniffingFileUrlsForHtml() override;
  void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                      int child_process_id) override;
  std::string GetApplicationLocale() override;
  std::string GetAcceptLangs(content::BrowserContext* context) override;
  gfx::ImageSkia GetDefaultFavicon() override;
  content::GeneratedCodeCacheSettings GetGeneratedCodeCacheSettings(
      content::BrowserContext* context) override;
  void AllowCertificateError(
      content::WebContents* web_contents,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      bool is_primary_main_frame_request,
      bool strict_enforcement,
      base::OnceCallback<void(content::CertificateRequestResultType)> callback)
      override;
  base::OnceClosure SelectClientCertificate(
      content::WebContents* web_contents,
      net::SSLCertRequestInfo* cert_request_info,
      net::ClientCertIdentityList client_certs,
      std::unique_ptr<content::ClientCertificateDelegate> delegate) override;
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
  base::FilePath GetDefaultDownloadDirectory() override;
  std::string GetDefaultDownloadName() override;
  void DidCreatePpapiPlugin(content::BrowserPpapiHost* browser_host) override;
  bool AllowPepperSocketAPI(
      content::BrowserContext* browser_context,
      const GURL& url,
      bool private_api,
      const content::SocketPermissionRequest* params) override;
  bool IsPepperVpnProviderAPIAllowed(content::BrowserContext* browser_context,
                                     const GURL& url) override;
  content::TracingDelegate* GetTracingDelegate() override;
  void GetAdditionalMappedFilesForChildProcess(
      const base::CommandLine& command_line,
      int child_process_id,
      content::PosixFileDescriptorInfo* mappings) override;
  void OverrideWebkitPrefs(content::WebContents* web_contents,
                           blink::web_pref::WebPreferences* web_prefs) override;
  std::vector<std::unique_ptr<content::NavigationThrottle>>
  CreateThrottlesForNavigation(
      content::NavigationHandle* navigation_handle) override;
  std::unique_ptr<content::DevToolsManagerDelegate>
  CreateDevToolsManagerDelegate() override;
  void RegisterAssociatedInterfaceBindersForRenderFrameHost(
      content::RenderFrameHost& render_frame_host,
      blink::AssociatedInterfaceRegistry& associated_registry) override;
  void ExposeInterfacesToRenderer(
      service_manager::BinderRegistry* registry,
      blink::AssociatedInterfaceRegistry* associated_registry,
      content::RenderProcessHost* render_process_host) override;
  void BindMediaServiceReceiver(content::RenderFrameHost* render_frame_host,
                                mojo::GenericPendingReceiver receiver) override;
  void RegisterBrowserInterfaceBindersForFrame(
      content::RenderFrameHost* render_frame_host,
      mojo::BinderMapWithContext<content::RenderFrameHost*>* map) override;
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
  CreateURLLoaderThrottles(
      const network::ResourceRequest& request,
      content::BrowserContext* browser_context,
      const base::RepeatingCallback<content::WebContents*()>& wc_getter,
      content::NavigationUIData* navigation_ui_data,
      int frame_tree_node_id) override;
  bool ShouldOverrideUrlLoading(int frame_tree_node_id,
                                bool browser_initiated,
                                const GURL& gurl,
                                const std::string& request_method,
                                bool has_user_gesture,
                                bool is_redirect,
                                bool is_outermost_main_frame,
                                ui::PageTransition transition,
                                bool* ignore_navigation) override;
  bool SupportsAvoidUnnecessaryBeforeUnloadCheckSync() override;
  bool CreateThreadPool(base::StringPiece name) override;
  std::unique_ptr<content::LoginDelegate> CreateLoginDelegate(
      const net::AuthChallengeInfo& auth_info,
      content::WebContents* web_contents,
      const content::GlobalRequestID& request_id,
      bool is_request_for_primary_main_frame,
      const GURL& url,
      scoped_refptr<net::HttpResponseHeaders> response_headers,
      bool first_auth_attempt,
      LoginAuthRequiredCallback auth_required_callback) override;
  bool HandleExternalProtocol(
      const GURL& url,
      content::WebContents::Getter web_contents_getter,
      int frame_tree_node_id,
      content::NavigationUIData* navigation_data,
      bool is_primary_main_frame,
      bool is_in_fenced_frame_tree,
      network::mojom::WebSandboxFlags sandbox_flags,
      ui::PageTransition page_transition,
      bool has_user_gesture,
      const absl::optional<url::Origin>& initiating_origin,
      content::RenderFrameHost* initiator_document,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>* out_factory)
      override;
  void RegisterNonNetworkSubresourceURLLoaderFactories(
      int render_process_id,
      int render_frame_id,
      const absl::optional<url::Origin>& request_initiator_origin,
      NonNetworkURLLoaderFactoryMap* factories) override;
  bool ShouldAllowNoLongerUsedProcessToExit() override;
  bool ShouldIsolateErrorPage(bool in_main_frame) override;
  bool ShouldEnableStrictSiteIsolation() override;
  bool ShouldDisableSiteIsolation(
      content::SiteIsolationMode site_isolation_mode) override;
  bool ShouldLockProcessToSite(content::BrowserContext* browser_context,
                               const GURL& effective_url) override;
  size_t GetMaxRendererProcessCountOverride() override;
  bool WillCreateURLLoaderFactory(
      content::BrowserContext* browser_context,
      content::RenderFrameHost* frame,
      int render_process_id,
      URLLoaderFactoryType type,
      const url::Origin& request_initiator,
      absl::optional<int64_t> navigation_id,
      ukm::SourceIdObj ukm_source_id,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>* factory_receiver,
      mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
          header_client,
      bool* bypass_redirect_checks,
      bool* disable_secure_dns,
      network::mojom::URLLoaderFactoryOverridePtr* factory_override) override;
  uint32_t GetWebSocketOptions(content::RenderFrameHost* frame) override;
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
  std::string GetProduct() override;
  std::string GetUserAgent() override;
  ContentBrowserClient::WideColorGamutHeuristic GetWideColorGamutHeuristic()
      override;
  void LogWebFeatureForCurrentPage(content::RenderFrameHost* render_frame_host,
                                   blink::mojom::WebFeature feature) override;
  bool ShouldAllowInsecureLocalNetworkRequests(
      content::BrowserContext* browser_context,
      const url::Origin& origin) override;
  content::SpeechRecognitionManagerDelegate*
  CreateSpeechRecognitionManagerDelegate() override;
  bool HasErrorPage(int http_status_code) override;
  bool SuppressDifferentOriginSubframeJSDialogs(
      content::BrowserContext* browser_context) override;
  bool ShouldPreconnectNavigation(
      content::BrowserContext* browser_context) override;
  void OnDisplayInsecureContent(content::WebContents* web_contents) override;
  // Allows the embedder to control if Attribution Reporting API operations can
  // happen in a given context.
  // For WebView Browser Attribution is explicitly disabled.
  bool IsAttributionReportingOperationAllowed(
      content::BrowserContext* browser_context,
      AttributionReportingOperation operation,
      content::RenderFrameHost* rfh,
      const url::Origin* source_origin,
      const url::Origin* destination_origin,
      const url::Origin* reporting_origin) override;

  AwFeatureListCreator* aw_feature_list_creator() {
    return aw_feature_list_creator_;
  }

  static void DisableCreatingThreadPool();

 private:
  scoped_refptr<safe_browsing::UrlCheckerDelegate>
  GetSafeBrowsingUrlCheckerDelegate();

  // Android WebView currently has a single global (non-off-the-record) browser
  // context.
  std::unique_ptr<AwBrowserContext> browser_context_;

  scoped_refptr<safe_browsing::UrlCheckerDelegate>
      safe_browsing_url_checker_delegate_;

  const bool sniff_file_urls_;

  // The AwFeatureListCreator is owned by AwMainDelegate.
  const raw_ptr<AwFeatureListCreator> aw_feature_list_creator_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_CONTENT_BROWSER_CLIENT_H_
