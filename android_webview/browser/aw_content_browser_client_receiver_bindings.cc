// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_content_browser_client.h"
#include "android_webview/browser/aw_print_manager.h"
#include "android_webview/browser/renderer_host/aw_render_view_host_ext.h"
#include "android_webview/browser/safe_browsing/aw_url_checker_delegate_impl.h"
#include "android_webview/common/aw_features.h"
#include "android_webview/common/mojom/render_message_filter.mojom.h"
#include "base/feature_list.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/cdm/browser/media_drm_storage_impl.h"
#include "components/content_capture/browser/onscreen_content_provider.h"
#include "components/network_hints/browser/simple_network_hints_handler_impl.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/mojo_safe_browsing_impl.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "media/mojo/buildflags.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/mojom/webview/webview_media_integrity.mojom.h"

#if BUILDFLAG(ENABLE_SPELLCHECK)
#include "components/spellcheck/browser/spell_check_host_impl.h"
#endif

namespace android_webview {

namespace {

#if BUILDFLAG(ENABLE_MOJO_CDM)
void CreateOriginId(cdm::MediaDrmStorageImpl::OriginIdObtainedCB callback) {
  std::move(callback).Run(true, base::UnguessableToken::Create());
}

void AllowEmptyOriginIdCB(base::OnceCallback<void(bool)> callback) {
  // Since CreateOriginId() always returns a non-empty origin ID, we don't need
  // to allow empty origin ID.
  std::move(callback).Run(false);
}

void CreateMediaDrmStorage(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<::media::mojom::MediaDrmStorage> receiver) {
  CHECK(render_frame_host);

  if (render_frame_host->GetLastCommittedOrigin().opaque()) {
    DVLOG(1) << __func__ << ": Unique origin.";
    return;
  }

  auto* aw_browser_context =
      static_cast<AwBrowserContext*>(render_frame_host->GetBrowserContext());
  DCHECK(aw_browser_context) << "AwBrowserContext not available.";

  PrefService* pref_service = aw_browser_context->GetPrefService();
  DCHECK(pref_service);

  // The object will be deleted on connection error, or when the frame navigates
  // away.
  new cdm::MediaDrmStorageImpl(
      *render_frame_host, pref_service, base::BindRepeating(&CreateOriginId),
      base::BindRepeating(&AllowEmptyOriginIdCB), std::move(receiver));
}
#endif  // BUILDFLAG(ENABLE_MOJO_CDM)

// Helper method that checks the RenderProcessHost is still alive before hopping
// over to the IO thread.
void MaybeCreateSafeBrowsing(
    int rph_id,
    base::RepeatingCallback<scoped_refptr<safe_browsing::UrlCheckerDelegate>()>
        get_checker_delegate,
    mojo::PendingReceiver<safe_browsing::mojom::SafeBrowsing> receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::RenderProcessHost* render_process_host =
      content::RenderProcessHost::FromID(rph_id);
  if (!render_process_host)
    return;

  safe_browsing::MojoSafeBrowsingImpl::MaybeCreate(
      rph_id, std::move(get_checker_delegate), std::move(receiver));
}

void BindNetworkHintsHandler(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<network_hints::mojom::NetworkHintsHandler> receiver) {
  network_hints::SimpleNetworkHintsHandlerImpl::Create(frame_host,
                                                       std::move(receiver));
}

// This class handles android_webview.mojom.RenderMessageFilter Mojo interface's
// methods on IO thread.
class AwContentsMessageFilter : public mojom::RenderMessageFilter {
 public:
  explicit AwContentsMessageFilter(int process_id);

  AwContentsMessageFilter(const AwContentsMessageFilter&) = delete;
  AwContentsMessageFilter& operator=(const AwContentsMessageFilter&) = delete;

  // mojom::RenderMessageFilter overrides:
  void SubFrameCreated(
      const blink::LocalFrameToken& parent_frame_token,
      const blink::LocalFrameToken& child_frame_token) override;

  ~AwContentsMessageFilter() override;

 private:
  const int process_id_;
};

AwContentsMessageFilter::AwContentsMessageFilter(int process_id)
    : process_id_(process_id) {}

AwContentsMessageFilter::~AwContentsMessageFilter() = default;

void AwContentsMessageFilter::SubFrameCreated(
    const blink::LocalFrameToken& parent_frame_token,
    const blink::LocalFrameToken& child_frame_token) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  AwContentsIoThreadClient::SubFrameCreated(process_id_, parent_frame_token,
                                            child_frame_token);
}

void CreateRenderMessageFilter(
    int rph_id,
    mojo::PendingReceiver<mojom::RenderMessageFilter> receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  mojo::MakeSelfOwnedReceiver(std::make_unique<AwContentsMessageFilter>(rph_id),
                              std::move(receiver));
}

template <typename Interface>
void ForwardToJavaFrame(content::RenderFrameHost* render_frame_host,
                        mojo::PendingReceiver<Interface> receiver) {
  render_frame_host->GetJavaInterfaces()->GetInterface(std::move(receiver));
}

void BindMediaIntegrityServiceReceiver(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::WebViewMediaIntegrityService>
        receiver) {
  const url::Origin& origin = render_frame_host->GetLastCommittedOrigin();
  // Note that this particular check respects the origin of the base URL
  // supplied by loadDataWithBaseURL.
  if ((origin.scheme() != url::kHttpScheme &&
       origin.scheme() != url::kHttpsScheme) ||
      !network::IsOriginPotentiallyTrustworthy(origin)) {
    mojo::ReportBadMessage(
        "Attempted to access WebView Media Integrity service for a "
        "non-trustworthy or non-HTTP/HTTPS origin.");
    return;
  };
  ForwardToJavaFrame<blink::mojom::WebViewMediaIntegrityService>(
      render_frame_host, std::move(receiver));
}

}  // anonymous namespace

void AwContentBrowserClient::BindMediaServiceReceiver(
    content::RenderFrameHost* render_frame_host,
    mojo::GenericPendingReceiver receiver) {
#if BUILDFLAG(ENABLE_MOJO_CDM)
  if (auto r = receiver.As<media::mojom::MediaDrmStorage>()) {
    CreateMediaDrmStorage(render_frame_host, std::move(r));
    return;
  }
#endif
}

void AwContentBrowserClient::
    RegisterAssociatedInterfaceBindersForRenderFrameHost(
        content::RenderFrameHost& render_frame_host,
        blink::AssociatedInterfaceRegistry& associated_registry) {
  associated_registry.AddInterface<autofill::mojom::AutofillDriver>(
      base::BindRepeating(
          &autofill::ContentAutofillDriverFactory::BindAutofillDriver,
          &render_frame_host));
  // TODO(lingqi): Swap the parameters so that lambda functions are not needed.
  associated_registry.AddInterface<
      content_capture::mojom::ContentCaptureReceiver>(base::BindRepeating(
      [](content::RenderFrameHost* render_frame_host,
         mojo::PendingAssociatedReceiver<
             content_capture::mojom::ContentCaptureReceiver> receiver) {
        content_capture::OnscreenContentProvider::BindContentCaptureReceiver(
            std::move(receiver), render_frame_host);
      },
      &render_frame_host));
  associated_registry.AddInterface<mojom::FrameHost>(base::BindRepeating(
      [](content::RenderFrameHost* render_frame_host,
         mojo::PendingAssociatedReceiver<mojom::FrameHost> receiver) {
        AwRenderViewHostExt::BindFrameHost(std::move(receiver),
                                           render_frame_host);
      },
      &render_frame_host));
  associated_registry.AddInterface<page_load_metrics::mojom::PageLoadMetrics>(
      base::BindRepeating(
          [](content::RenderFrameHost* render_frame_host,
             mojo::PendingAssociatedReceiver<
                 page_load_metrics::mojom::PageLoadMetrics> receiver) {
            page_load_metrics::MetricsWebContentsObserver::BindPageLoadMetrics(
                std::move(receiver), render_frame_host);
          },
          &render_frame_host));
  associated_registry.AddInterface<printing::mojom::PrintManagerHost>(
      base::BindRepeating(
          [](content::RenderFrameHost* render_frame_host,
             mojo::PendingAssociatedReceiver<printing::mojom::PrintManagerHost>
                 receiver) {
            AwPrintManager::BindPrintManagerHost(std::move(receiver),
                                                 render_frame_host);
          },
          &render_frame_host));
  associated_registry.AddInterface<
      security_interstitials::mojom::InterstitialCommands>(base::BindRepeating(
      [](content::RenderFrameHost* render_frame_host,
         mojo::PendingAssociatedReceiver<
             security_interstitials::mojom::InterstitialCommands> receiver) {
        security_interstitials::SecurityInterstitialTabHelper::
            BindInterstitialCommands(std::move(receiver), render_frame_host);
      },
      &render_frame_host));
}

void AwContentBrowserClient::ExposeInterfacesToRenderer(
    service_manager::BinderRegistry* registry,
    blink::AssociatedInterfaceRegistry* associated_registry,
    content::RenderProcessHost* render_process_host) {
  registry->AddInterface<safe_browsing::mojom::SafeBrowsing>(
      base::BindRepeating(
          &MaybeCreateSafeBrowsing, render_process_host->GetID(),
          base::BindRepeating(
              &AwContentBrowserClient::GetSafeBrowsingUrlCheckerDelegate,
              base::Unretained(this))),
      content::GetUIThreadTaskRunner({}));

  // Add the RenderMessageFilter creation callback, the callbkack will happen on
  // the IO thread.
  registry->AddInterface<mojom::RenderMessageFilter>(base::BindRepeating(
      &CreateRenderMessageFilter, render_process_host->GetID()));
}

void AwContentBrowserClient::RegisterBrowserInterfaceBindersForFrame(
    content::RenderFrameHost* render_frame_host,
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map) {
  map->Add<network_hints::mojom::NetworkHintsHandler>(
      base::BindRepeating(&BindNetworkHintsHandler));

#if BUILDFLAG(ENABLE_SPELLCHECK)
  auto create_spellcheck_host =
      [](content::RenderFrameHost* render_frame_host,
         mojo::PendingReceiver<spellcheck::mojom::SpellCheckHost> receiver) {
        mojo::MakeSelfOwnedReceiver(std::make_unique<SpellCheckHostImpl>(),
                                    std::move(receiver));
      };
  map->Add<spellcheck::mojom::SpellCheckHost>(
      base::BindRepeating(create_spellcheck_host),
      content::GetUIThreadTaskRunner({}));
#endif

  if (base::FeatureList::IsEnabled(
          features::kWebViewMediaIntegrityApiBlinkExtension)) {
    map->Add<blink::mojom::WebViewMediaIntegrityService>(
        base::BindRepeating(&BindMediaIntegrityServiceReceiver));
  }
}

void AwContentBrowserClient::
    RegisterMojoBinderPoliciesForSameOriginPrerendering(
        content::MojoBinderPolicyMap& policy_map) {
  policy_map.SetAssociatedPolicy<page_load_metrics::mojom::PageLoadMetrics>(
      content::MojoBinderAssociatedPolicy::kGrant);
}

}  // namespace android_webview
