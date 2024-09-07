// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_response_interceptor_url_loader_throttle.h"

#include <tuple>
#include <utility>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/numerics/safe_conversions.h"
#include "base/unguessable_token.h"
#include "base/uuid.h"
#include "chrome/browser/extensions/api/streams_private/streams_private_api.h"
#include "chrome/browser/plugins/plugin_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_utils.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_attach_helper.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "pdf/buildflags.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/mojom/loader/transferrable_url_loader.mojom.h"

#if BUILDFLAG(ENABLE_PDF)
#include "components/pdf/common/constants.h"
#include "pdf/pdf_features.h"
#endif  // BUILDFLAG(ENABLE_PDF)

namespace {

void ClearAllButFrameAncestors(network::mojom::URLResponseHead* response_head) {
  response_head->headers->RemoveHeader("Content-Security-Policy");
  response_head->headers->RemoveHeader("Content-Security-Policy-Report-Only");

  if (!response_head->parsed_headers)
    return;

  std::vector<network::mojom::ContentSecurityPolicyPtr>& csp =
      response_head->parsed_headers->content_security_policy;
  std::vector<network::mojom::ContentSecurityPolicyPtr> cleared;

  for (auto& policy : csp) {
    auto frame_ancestors = policy->directives.find(
        network::mojom::CSPDirectiveName::FrameAncestors);
    if (frame_ancestors == policy->directives.end())
      continue;

    auto cleared_policy = network::mojom::ContentSecurityPolicy::New();
    cleared_policy->self_origin = std::move(policy->self_origin);
    cleared_policy->header = std::move(policy->header);
    cleared_policy->header->header_value = "";
    cleared_policy
        ->directives[network::mojom::CSPDirectiveName::FrameAncestors] =
        std::move(frame_ancestors->second);

    auto raw_frame_ancestors = policy->raw_directives.find(
        network::mojom::CSPDirectiveName::FrameAncestors);
    if (raw_frame_ancestors == policy->raw_directives.end()) {
      DCHECK(false);
    } else {
      cleared_policy->header->header_value =
          "frame-ancestors " + raw_frame_ancestors->second;
      response_head->headers->AddHeader(
          cleared_policy->header->type ==
                  network::mojom::ContentSecurityPolicyType::kEnforce
              ? "Content-Security-Policy"
              : "Content-Security-Policy-Report-Only",
          cleared_policy->header->header_value);
      cleared_policy
          ->raw_directives[network::mojom::CSPDirectiveName::FrameAncestors] =
          std::move(raw_frame_ancestors->second);
    }

    cleared.push_back(std::move(cleared_policy));
  }

  csp.swap(cleared);
}

}  // namespace

PluginResponseInterceptorURLLoaderThrottle::
    PluginResponseInterceptorURLLoaderThrottle(
        network::mojom::RequestDestination request_destination,
        content::FrameTreeNodeId frame_tree_node_id)
    : request_destination_(request_destination),
      frame_tree_node_id_(frame_tree_node_id) {}

PluginResponseInterceptorURLLoaderThrottle::
    ~PluginResponseInterceptorURLLoaderThrottle() = default;

void PluginResponseInterceptorURLLoaderThrottle::WillProcessResponse(
    const GURL& response_url,
    network::mojom::URLResponseHead* response_head,
    bool* defer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::WebContents* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id_);
  if (!web_contents)
    return;

  if (content::download_utils::MustDownload(
          web_contents->GetBrowserContext(), response_url,
          response_head->headers.get(), response_head->mime_type)) {
    return;
  }

  std::string extension_id = PluginUtils::GetExtensionIdForMimeType(
      web_contents->GetBrowserContext(), response_head->mime_type);

  if (extension_id.empty())
    return;

  // TODO(crbug.com/40180674): Support prerendering of MimeHandlerViews.
  if (web_contents->IsPrerenderedFrame(frame_tree_node_id_)) {
    delegate_->CancelWithError(
        net::Error::ERR_BLOCKED_BY_CLIENT,
        "MimeHandler prerendering support not implemented.");
    return;
  }

  // Chrome's PDF Extension does not work properly in the face of a restrictive
  // Content-Security-Policy, and does not currently respect the policy anyway.
  // Ignore CSP served on a PDF response. https://crbug.com/271452
  if (extension_id == extension_misc::kPdfExtensionId &&
      response_head->headers) {
    // We still want to honor the frame-ancestors directive in the
    // AncestorThrottle.
    ClearAllButFrameAncestors(response_head);
  }

  // TODO(mcnee): Could this id just be an int instead? This is only used
  // internally.
  const std::string stream_id =
      base::Uuid::GenerateRandomV4().AsLowercaseString();

  mojo::PendingRemote<network::mojom::URLLoader> dummy_new_loader;
  std::ignore = dummy_new_loader.InitWithNewPipeAndPassReceiver();
  mojo::Remote<network::mojom::URLLoaderClient> new_client;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> new_client_receiver =
      new_client.BindNewPipeAndPassReceiver();

  std::string payload;
  const std::string internal_id = base::UnguessableToken::Create().ToString();

#if BUILDFLAG(ENABLE_PDF)
  const bool is_for_oopif_pdf = chrome_pdf::features::IsOopifPdfEnabled() &&
                                response_head->mime_type == pdf::kPDFMimeType;
#else
  constexpr bool is_for_oopif_pdf = false;
#endif
  if (is_for_oopif_pdf) {
    // For the PDF viewer, set the payload without creating a MimeHandlerView.
    payload =
        extensions::MimeHandlerViewAttachHelper::CreateTemplateMimeHandlerPage(
            response_url, response_head->mime_type, internal_id);
  } else {
    // The resource is handled by frame-based MimeHandlerView, so let the
    // MimeHandlerView code set the payload.
    payload = extensions::MimeHandlerViewAttachHelper::
        OverrideBodyForInterceptedResponse(
            frame_tree_node_id_, response_url, response_head->mime_type,
            stream_id, internal_id,
            base::BindOnce(
                &PluginResponseInterceptorURLLoaderThrottle::ResumeLoad,
                weak_factory_.GetWeakPtr()));
  }
  *defer = true;

  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  CHECK_EQ(
      mojo::CreateDataPipe(payload.size(), producer_handle, consumer_handle),
      MOJO_RESULT_OK);

  CHECK_EQ(MOJO_RESULT_OK,
           producer_handle->WriteAllData(base::as_byte_span(payload)));

  network::URLLoaderCompletionStatus status(net::OK);
  status.decoded_body_length = base::checked_cast<int64_t>(payload.size());
  new_client->OnComplete(status);

  mojo::PendingRemote<network::mojom::URLLoader> original_loader;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> original_client;
  delegate_->InterceptResponse(std::move(dummy_new_loader),
                               std::move(new_client_receiver), &original_loader,
                               &original_client, &consumer_handle);

  // Make a deep copy of URLResponseHead before passing it cross-thread.
  auto deep_copied_response = response_head->Clone();
  if (response_head->headers) {
    deep_copied_response->headers =
        base::MakeRefCounted<net::HttpResponseHeaders>(
            response_head->headers->raw_headers());
  }

  auto transferrable_loader = blink::mojom::TransferrableURLLoader::New();
  transferrable_loader->url = GURL(
      extensions::Extension::GetBaseURLFromExtensionId(extension_id).spec() +
      base::Uuid::GenerateRandomV4().AsLowercaseString());
  transferrable_loader->url_loader = std::move(original_loader);
  transferrable_loader->url_loader_client = std::move(original_client);
  transferrable_loader->head = std::move(deep_copied_response);
  transferrable_loader->head->intercepted_by_plugin = true;
  transferrable_loader->body = std::move(consumer_handle);

  bool embedded =
      request_destination_ != network::mojom::RequestDestination::kDocument;
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &extensions::StreamsPrivateAPI::SendExecuteMimeTypeHandlerEvent,
          extension_id, stream_id, embedded, frame_tree_node_id_,
          std::move(transferrable_loader), response_url, internal_id));

#if BUILDFLAG(ENABLE_PDF)
  if (is_for_oopif_pdf) {
    // Schedule `ResumeLoad()` for after the SendExecuteMimeTypeHandlerEvent()
    // call, to ensure the work in SendExecuteMimeTypeHandlerEvent() does not
    // race against subsequent network events.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&PluginResponseInterceptorURLLoaderThrottle::ResumeLoad,
                       weak_factory_.GetWeakPtr()));
  }
#endif
}

void PluginResponseInterceptorURLLoaderThrottle::ResumeLoad() {
  delegate_->Resume();
}
