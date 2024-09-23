// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_RENDERER_AW_CONTENT_RENDERER_CLIENT_H_
#define ANDROID_WEBVIEW_RENDERER_AW_CONTENT_RENDERER_CLIENT_H_

#include <memory>
#include <string>
#include <string_view>

#include "android_webview/common/mojom/render_message_filter.mojom.h"
#include "android_webview/renderer/aw_render_thread_observer.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "content/public/renderer/content_renderer_client.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/local_interface_provider.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"

#if BUILDFLAG(ENABLE_SPELLCHECK)
class SpellCheck;
#endif

namespace visitedlink {
class VisitedLinkReader;
}

namespace android_webview {

class AwContentRendererClient : public content::ContentRendererClient,
                                public service_manager::LocalInterfaceProvider {
 public:
  AwContentRendererClient();

  AwContentRendererClient(const AwContentRendererClient&) = delete;
  AwContentRendererClient& operator=(const AwContentRendererClient&) = delete;

  ~AwContentRendererClient() override;

  // ContentRendererClient implementation.
  void RenderThreadStarted() override;
  void ExposeInterfacesToBrowser(mojo::BinderMap* binders) override;
  void RenderFrameCreated(content::RenderFrame* render_frame) override;
  void WebViewCreated(blink::WebView* web_view,
                      bool was_created_by_renderer,
                      const url::Origin* outermost_origin) override;
  void PrepareErrorPage(content::RenderFrame* render_frame,
                        const blink::WebURLError& error,
                        const std::string& http_method,
                        content::mojom::AlternativeErrorPageOverrideInfoPtr
                            alternative_error_page_info,
                        std::string* error_html) override;
  uint64_t VisitedLinkHash(std::string_view canonical_url) override;
  uint64_t PartitionedVisitedLinkFingerprint(
      std::string_view canonical_link_url,
      const net::SchemefulSite& top_level_site,
      const url::Origin& frame_origin) override;
  bool IsLinkVisited(uint64_t link_hash) override;
  void AddOrUpdateVisitedLinkSalt(const url::Origin& origin,
                                  uint64_t salt) override;
  void RunScriptsAtDocumentStart(content::RenderFrame* render_frame) override;
  std::unique_ptr<media::KeySystemSupportRegistration> GetSupportedKeySystems(
      content::RenderFrame* render_frame,
      media::GetSupportedKeySystemsCB cb) override;
  bool HandleNavigation(content::RenderFrame* render_frame,
                        blink::WebFrame* frame,
                        const blink::WebURLRequest& request,
                        blink::WebNavigationType type,
                        blink::WebNavigationPolicy default_policy,
                        bool is_redirect) override;
  std::unique_ptr<blink::WebPrescientNetworking> CreatePrescientNetworking(
      content::RenderFrame* render_frame) override;
  void SetRuntimeFeaturesDefaultsBeforeBlinkInitialization() override;
  std::unique_ptr<blink::URLLoaderThrottleProvider>
  CreateURLLoaderThrottleProvider(
      blink::URLLoaderThrottleProviderType provider_type) override;

  visitedlink::VisitedLinkReader* visited_link_reader() {
    return visited_link_reader_.get();
  }

 private:
  // service_manager::LocalInterfaceProvider:
  void GetInterface(const std::string& name,
                    mojo::ScopedMessagePipeHandle request_handle) override;

  mojom::RenderMessageFilter* GetRenderMessageFilter();

  std::unique_ptr<AwRenderThreadObserver> aw_render_thread_observer_;
  std::unique_ptr<visitedlink::VisitedLinkReader> visited_link_reader_;

  scoped_refptr<blink::ThreadSafeBrowserInterfaceBrokerProxy>
      browser_interface_broker_;

  mojo::Remote<mojom::RenderMessageFilter> render_message_filter_;

#if BUILDFLAG(ENABLE_SPELLCHECK)
  std::unique_ptr<SpellCheck> spellcheck_;
#endif
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_RENDERER_AW_CONTENT_RENDERER_CLIENT_H_
