// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/renderer/aw_content_renderer_client.h"

#include <memory>
#include <vector>

#include "android_webview/common/aw_switches.h"
#include "android_webview/common/render_view_messages.h"
#include "android_webview/common/url_constants.h"
#include "android_webview/grit/aw_resources.h"
#include "android_webview/grit/aw_strings.h"
#include "android_webview/renderer/aw_content_settings_client.h"
#include "android_webview/renderer/aw_key_systems.h"
#include "android_webview/renderer/aw_print_render_frame_helper_delegate.h"
#include "android_webview/renderer/aw_render_frame_ext.h"
#include "android_webview/renderer/aw_render_view_ext.h"
#include "android_webview/renderer/aw_url_loader_throttle_provider.h"
#include "android_webview/renderer/aw_websocket_handshake_throttle_provider.h"
#include "android_webview/renderer/print_render_frame_observer.h"
#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/printing/renderer/print_render_frame_helper.h"
#include "components/supervised_user_error_page/gin_wrapper.h"
#include "components/supervised_user_error_page/supervised_user_error_page_android.h"
#include "components/visitedlink/renderer/visitedlink_slave.h"
#include "components/web_restrictions/interfaces/web_restrictions.mojom.h"
#include "content/public/child/child_thread.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/common/simple_connection_filter.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "net/base/escape.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/features.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_navigation_type.h"
#include "third_party/blink/public/web/web_security_policy.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if BUILDFLAG(ENABLE_SPELLCHECK)
#include "components/spellcheck/renderer/spellcheck.h"
#include "components/spellcheck/renderer/spellcheck_provider.h"
#endif

using content::RenderThread;

namespace android_webview {

namespace {
constexpr char kThrottledErrorDescription[] =
    "Request throttled. Visit http://dev.chromium.org/throttling for more "
    "information.";
}  // namespace

AwContentRendererClient::AwContentRendererClient() {}

AwContentRendererClient::~AwContentRendererClient() {}

void AwContentRendererClient::RenderThreadStarted() {
  RenderThread* thread = RenderThread::Get();
  aw_render_thread_observer_.reset(new AwRenderThreadObserver);
  thread->AddObserver(aw_render_thread_observer_.get());

  visited_link_slave_.reset(new visitedlink::VisitedLinkSlave);

  auto registry = std::make_unique<service_manager::BinderRegistry>();
  registry->AddInterface(visited_link_slave_->GetBindCallback(),
                         base::ThreadTaskRunnerHandle::Get());
  content::ChildThread::Get()
      ->GetServiceManagerConnection()
      ->AddConnectionFilter(std::make_unique<content::SimpleConnectionFilter>(
          std::move(registry)));

#if BUILDFLAG(ENABLE_SPELLCHECK)
  if (!spellcheck_)
    spellcheck_ = std::make_unique<SpellCheck>(nullptr, this);
#endif
}

bool AwContentRendererClient::HandleNavigation(
    content::RenderFrame* render_frame,
    bool is_content_initiated,
    bool render_view_was_created_by_renderer,
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
  // TODO(sgurun) is_content_initiated is normally false for cross-origin
  // navigations but since android_webview does not swap out renderers, this
  // works fine. This will stop working if android_webview starts swapping out
  // renderers on navigation.
  bool application_initiated =
      !is_content_initiated || type == blink::kWebNavigationTypeBackForward;

  // Don't offer application-initiated navigations unless it's a redirect.
  if (application_initiated && !is_redirect)
    return false;

  bool is_main_frame = !frame->Parent();
  const GURL& gurl = request.Url();
  // For HTTP schemes, only top-level navigations can be overridden. Similarly,
  // WebView Classic lets app override only top level about:blank navigations.
  // So we filter out non-top about:blank navigations here.
  if (!is_main_frame &&
      (gurl.SchemeIs(url::kHttpScheme) || gurl.SchemeIs(url::kHttpsScheme) ||
       gurl.SchemeIs(url::kAboutScheme)))
    return false;

  // use NavigationInterception throttle to handle the call as that can
  // be deferred until after the java side has been constructed.
  //
  // TODO(nick): |render_view_was_created_by_renderer| was plumbed in to
  // preserve the existing code behavior, but it doesn't appear to be correct.
  // In particular, this value will be true for the initial navigation of a
  // RenderView created via window.open(), but it will also be true for all
  // subsequent navigations in that RenderView, no matter how they are
  // initiated.
  if (render_view_was_created_by_renderer) {
    return false;
  }

  bool ignore_navigation = false;
  base::string16 url = request.Url().GetString().Utf16();
  bool has_user_gesture = request.HasUserGesture();

  int render_frame_id = render_frame->GetRoutingID();
  RenderThread::Get()->Send(new AwViewHostMsg_ShouldOverrideUrlLoading(
      render_frame_id, url, has_user_gesture, is_redirect, is_main_frame,
      &ignore_navigation));
  return ignore_navigation;
}

void AwContentRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame) {
  new AwContentSettingsClient(render_frame);
  new PrintRenderFrameObserver(render_frame);
  new printing::PrintRenderFrameHelper(
      render_frame, std::make_unique<AwPrintRenderFrameHelperDelegate>());
  new AwRenderFrameExt(render_frame);

  // TODO(jam): when the frame tree moves into content and parent() works at
  // RenderFrame construction, simplify this by just checking parent().
  content::RenderFrame* parent_frame =
      render_frame->GetRenderView()->GetMainRenderFrame();
  if (parent_frame && parent_frame != render_frame) {
    // Avoid any race conditions from having the browser's UI thread tell the IO
    // thread that a subframe was created.
    RenderThread::Get()->Send(new AwViewHostMsg_SubFrameCreated(
        parent_frame->GetRoutingID(), render_frame->GetRoutingID()));
  }

#if BUILDFLAG(ENABLE_SPELLCHECK)
  new SpellCheckProvider(render_frame, spellcheck_.get(), this);
#endif
}

void AwContentRendererClient::RenderViewCreated(
    content::RenderView* render_view) {
  AwRenderViewExt::RenderViewCreated(render_view);
}

bool AwContentRendererClient::HasErrorPage(int http_status_code) {
  return http_status_code >= 400;
}

void AwContentRendererClient::PrepareErrorPage(
    content::RenderFrame* render_frame,
    const blink::WebURLRequest& failed_request,
    const blink::WebURLError& error,
    std::string* error_html,
    base::string16* error_description) {
  std::string err;
  if (error.reason() == net::ERR_TEMPORARILY_THROTTLED)
    err = kThrottledErrorDescription;
  else
    err = net::ErrorToString(error.reason());
  if (error_description)
    *error_description = base::ASCIIToUTF16(err);

  if (!error_html)
    return;

  // Create the error page based on the error reason.
  GURL gurl(failed_request.Url());
  std::string url_string = gurl.possibly_invalid_spec();
  int reason_id = IDS_AW_WEBPAGE_CAN_NOT_BE_LOADED;

  if (error.reason() == net::ERR_BLOCKED_BY_ADMINISTRATOR) {
    // This creates a different error page giving considerably more
    // detail, and possibly allowing the user to request access.
    // Get the details this needs from the browser.
    render_frame->GetRemoteInterfaces()->GetInterface(
        &web_restrictions_service_);
    web_restrictions::mojom::ClientResultPtr result;
    if (web_restrictions_service_->GetResult(url_string, &result)) {
      std::string detailed_error_html =
          supervised_user_error_page::BuildHtmlFromWebRestrictionsResult(
              result, RenderThread::Get()->GetLocale());
      if (!detailed_error_html.empty()) {
        *error_html = detailed_error_html;
        supervised_user_error_page::GinWrapper::InstallWhenFrameReady(
            render_frame, url_string, web_restrictions_service_);
        return;
      }
      // If the error page isn't available (it is only available in
      // Monochrome) but the user is a child then we want to give a simple
      // custom message.
      if (result->intParams["Is child account"])
        reason_id = IDS_AW_WEBPAGE_PARENTAL_PERMISSION_NEEDED;
    }
  }

  if (err.empty())
    reason_id = IDS_AW_WEBPAGE_TEMPORARILY_DOWN;

  std::string escaped_url = net::EscapeForHTML(url_string);
  std::vector<std::string> replacements;
  replacements.push_back(
      l10n_util::GetStringUTF8(IDS_AW_WEBPAGE_NOT_AVAILABLE));
  replacements.push_back(
      l10n_util::GetStringFUTF8(reason_id, base::UTF8ToUTF16(escaped_url)));

  // Having chosen the base reason, chose what extra information to add.
  if (reason_id == IDS_AW_WEBPAGE_PARENTAL_PERMISSION_NEEDED) {
    replacements.push_back("");
  } else if (reason_id == IDS_AW_WEBPAGE_TEMPORARILY_DOWN) {
    replacements.push_back(
        l10n_util::GetStringUTF8(IDS_AW_WEBPAGE_TEMPORARILY_DOWN_SUGGESTIONS));
  } else {
    replacements.push_back(err);
  }
  if (base::i18n::IsRTL())
    replacements.push_back("direction: rtl;");
  else
    replacements.push_back("");
  *error_html = base::ReplaceStringPlaceholders(
      ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_AW_LOAD_ERROR_HTML),
      replacements, nullptr);
}

unsigned long long AwContentRendererClient::VisitedLinkHash(
    const char* canonical_url,
    size_t length) {
  return visited_link_slave_->ComputeURLFingerprint(canonical_url, length);
}

bool AwContentRendererClient::IsLinkVisited(unsigned long long link_hash) {
  return visited_link_slave_->IsVisited(link_hash);
}

void AwContentRendererClient::AddSupportedKeySystems(
    std::vector<std::unique_ptr<::media::KeySystemProperties>>* key_systems) {
  AwAddKeySystems(key_systems);
}

std::unique_ptr<content::WebSocketHandshakeThrottleProvider>
AwContentRendererClient::CreateWebSocketHandshakeThrottleProvider() {
  return std::make_unique<AwWebSocketHandshakeThrottleProvider>();
}

std::unique_ptr<content::URLLoaderThrottleProvider>
AwContentRendererClient::CreateURLLoaderThrottleProvider(
    content::URLLoaderThrottleProviderType provider_type) {
  return std::make_unique<AwURLLoaderThrottleProvider>(provider_type);
}

void AwContentRendererClient::GetInterface(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  // A dirty hack to make SpellCheckHost requests work on WebView.
  // TODO(crbug.com/806394): Use a WebView-specific service for SpellCheckHost
  // and SafeBrowsing, instead of |content_browser|.
  RenderThread::Get()->GetConnector()->BindInterface(
      service_manager::Identity(content::mojom::kBrowserServiceName),
      interface_name, std::move(interface_pipe));
}

}  // namespace android_webview
