// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/renderer/aw_content_renderer_client.h"

#include <memory>
#include <vector>

#include "android_webview/common/aw_switches.h"
#include "android_webview/common/mojom/frame.mojom.h"
#include "android_webview/common/url_constants.h"
#include "android_webview/renderer/aw_content_settings_client.h"
#include "android_webview/renderer/aw_print_render_frame_helper_delegate.h"
#include "android_webview/renderer/aw_render_frame_ext.h"
#include "android_webview/renderer/aw_render_view_ext.h"
#include "android_webview/renderer/aw_safe_browsing_error_page_controller_delegate_impl.h"
#include "android_webview/renderer/aw_url_loader_throttle_provider.h"
#include "android_webview/renderer/aw_websocket_handshake_throttle_provider.h"
#include "android_webview/renderer/browser_exposed_renderer_interfaces.h"
#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "components/android_system_error_page/error_page_populator.h"
#include "components/cdm/renderer/key_system_support_update.h"
#include "components/js_injection/renderer/js_communication.h"
#include "components/network_hints/renderer/web_prescient_networking_impl.h"
#include "components/page_load_metrics/renderer/metrics_render_frame_observer.h"
#include "components/printing/renderer/print_render_frame_helper.h"
#include "components/visitedlink/renderer/visitedlink_reader.h"
#include "content/public/child/child_thread.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "ipc/ipc_sync_channel.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_navigation_type.h"
#include "third_party/blink/public/web/web_security_policy.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if BUILDFLAG(ENABLE_SPELLCHECK)
#include "components/spellcheck/renderer/spellcheck.h"
#include "components/spellcheck/renderer/spellcheck_provider.h"
#endif

using content::RenderThread;

namespace android_webview {

AwContentRendererClient::AwContentRendererClient() = default;

AwContentRendererClient::~AwContentRendererClient() = default;

void AwContentRendererClient::RenderThreadStarted() {
  RenderThread* thread = RenderThread::Get();
  aw_render_thread_observer_ = std::make_unique<AwRenderThreadObserver>();
  thread->AddObserver(aw_render_thread_observer_.get());

  visited_link_reader_ = std::make_unique<visitedlink::VisitedLinkReader>();

  browser_interface_broker_ =
      blink::Platform::Current()->GetBrowserInterfaceBroker();

#if BUILDFLAG(ENABLE_SPELLCHECK)
  if (!spellcheck_)
    spellcheck_ = std::make_unique<SpellCheck>(this);
#endif
}

void AwContentRendererClient::ExposeInterfacesToBrowser(
    mojo::BinderMap* binders) {
  // NOTE: Do not add binders directly within this method. Instead, modify the
  // definition of |ExposeRendererInterfacesToBrowser()| to ensure security
  // review coverage.
  ExposeRendererInterfacesToBrowser(this, binders);
}

bool AwContentRendererClient::HandleNavigation(
    content::RenderFrame* render_frame,
    blink::WebFrame* frame,
    const blink::WebURLRequest& request,
    blink::WebNavigationType type,
    blink::WebNavigationPolicy default_policy,
    bool is_redirect) {
  // Only GETs can be overridden.
  if (!request.HttpMethod().Equals("GET"))
    return false;

  // Any navigation from loadUrl, and goBack/Forward are considered application-
  // initiated and hence will not yield a shouldOverrideUrlLoading() callback.
  // Webview classic does not consider reload application-initiated so we
  // continue the same behavior.
  bool application_initiated = type == blink::kWebNavigationTypeBackForward;

  // Don't offer application-initiated navigations unless it's a redirect.
  if (application_initiated && !is_redirect)
    return false;

  bool is_outermost_main_frame = frame->IsOutermostMainFrame();
  const GURL& gurl = request.Url();
  // For HTTP schemes, only top-level navigations can be overridden. Similarly,
  // WebView Classic lets app override only top level about:blank navigations.
  // So we filter out non-top about:blank navigations here.
  if (!is_outermost_main_frame &&
      (gurl.SchemeIs(url::kHttpScheme) || gurl.SchemeIs(url::kHttpsScheme) ||
       gurl.SchemeIs(url::kAboutScheme)))
    return false;

  AwRenderViewExt* view =
      AwRenderViewExt::FromWebView(render_frame->GetWebView());

  // use NavigationInterception throttle to handle the call as that can
  // be deferred until after the java side has been constructed.
  //
  // TODO(nick): `view->created_by_renderer()` was plumbed in to
  // preserve the existing code behavior, but it doesn't appear to be correct.
  // In particular, this value will be true for the initial navigation of a
  // RenderView created via window.open(), but it will also be true for all
  // subsequent navigations in that RenderView, no matter how they are
  // initiated.
  if (view->created_by_renderer()) {
    return false;
  }

  bool ignore_navigation = false;
  std::u16string url = request.Url().GetString().Utf16();
  bool has_user_gesture = request.HasUserGesture();

  mojo::AssociatedRemote<mojom::FrameHost> frame_host_remote;
  render_frame->GetRemoteAssociatedInterfaces()->GetInterface(
      &frame_host_remote);
  frame_host_remote->ShouldOverrideUrlLoading(
      url, has_user_gesture, is_redirect, is_outermost_main_frame,
      &ignore_navigation);

  return ignore_navigation;
}

void AwContentRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame) {
  new AwContentSettingsClient(render_frame);
  new printing::PrintRenderFrameHelper(
      render_frame, std::make_unique<AwPrintRenderFrameHelperDelegate>());
  new AwRenderFrameExt(render_frame);
  new js_injection::JsCommunication(render_frame);
  new AwSafeBrowsingErrorPageControllerDelegateImpl(render_frame);

  content::RenderFrame* main_frame = render_frame->GetMainRenderFrame();
  if (main_frame && main_frame != render_frame) {
    // Avoid any race conditions from having the browser's UI thread tell the IO
    // thread that a subframe was created.
    GetRenderMessageFilter()->SubFrameCreated(main_frame->GetRoutingID(),
                                              render_frame->GetRoutingID());
  }

#if BUILDFLAG(ENABLE_SPELLCHECK)
  new SpellCheckProvider(render_frame, spellcheck_.get(), this);
#endif

  // Owned by |render_frame|.
  new page_load_metrics::MetricsRenderFrameObserver(render_frame);
}

std::unique_ptr<blink::WebPrescientNetworking>
AwContentRendererClient::CreatePrescientNetworking(
    content::RenderFrame* render_frame) {
  return std::make_unique<network_hints::WebPrescientNetworkingImpl>(
      render_frame);
}

void AwContentRendererClient::WebViewCreated(
    blink::WebView* web_view,
    bool was_created_by_renderer,
    const url::Origin* outermost_origin) {
  AwRenderViewExt::WebViewCreated(web_view, was_created_by_renderer);
}

void AwContentRendererClient::PrepareErrorPage(
    content::RenderFrame* render_frame,
    const blink::WebURLError& error,
    const std::string& http_method,
    content::mojom::AlternativeErrorPageOverrideInfoPtr
        alternative_error_page_info,
    std::string* error_html) {
  AwSafeBrowsingErrorPageControllerDelegateImpl::Get(render_frame)
      ->PrepareForErrorPage();

  android_system_error_page::PopulateErrorPageHtml(error, error_html);
}

uint64_t AwContentRendererClient::VisitedLinkHash(const char* canonical_url,
                                                  size_t length) {
  return visited_link_reader_->ComputeURLFingerprint(canonical_url, length);
}

bool AwContentRendererClient::IsLinkVisited(uint64_t link_hash) {
  return visited_link_reader_->IsVisited(link_hash);
}

void AwContentRendererClient::RunScriptsAtDocumentStart(
    content::RenderFrame* render_frame) {
  js_injection::JsCommunication* communication =
      js_injection::JsCommunication::Get(render_frame);
  communication->RunScriptsAtDocumentStart();
}

void AwContentRendererClient::GetSupportedKeySystems(
    media::GetSupportedKeySystemsCB cb) {
  // WebView always allows persisting data.
  cdm::GetSupportedKeySystemsUpdates(/*can_persist_data=*/true, std::move(cb));
}

std::unique_ptr<blink::WebSocketHandshakeThrottleProvider>
AwContentRendererClient::CreateWebSocketHandshakeThrottleProvider() {
  return std::make_unique<AwWebSocketHandshakeThrottleProvider>(
      browser_interface_broker_.get());
}

std::unique_ptr<blink::URLLoaderThrottleProvider>
AwContentRendererClient::CreateURLLoaderThrottleProvider(
    blink::URLLoaderThrottleProviderType provider_type) {
  return std::make_unique<AwURLLoaderThrottleProvider>(
      browser_interface_broker_.get(), provider_type);
}

void AwContentRendererClient::GetInterface(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  // A dirty hack to make SpellCheckHost requests work on WebView.
  // TODO(crbug.com/806394): Use a WebView-specific service for SpellCheckHost
  // and SafeBrowsing, instead of |content_browser|.
  RenderThread::Get()->BindHostReceiver(
      mojo::GenericPendingReceiver(interface_name, std::move(interface_pipe)));
}

mojom::RenderMessageFilter* AwContentRendererClient::GetRenderMessageFilter() {
  if (!render_message_filter_) {
    RenderThread::Get()->GetChannel()->GetRemoteAssociatedInterface(
        &render_message_filter_);
  }
  return render_message_filter_.get();
}

}  // namespace android_webview
