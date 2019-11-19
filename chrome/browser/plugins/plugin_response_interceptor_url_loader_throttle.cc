// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_response_interceptor_url_loader_throttle.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/guid.h"
#include "base/task/post_task.h"
#include "chrome/browser/extensions/api/streams_private/streams_private_api.h"
#include "chrome/browser/plugins/plugin_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/resource_type.h"
#include "content/public/common/transferrable_url_loader.mojom.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_attach_helper.h"
#include "extensions/common/extension.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

PluginResponseInterceptorURLLoaderThrottle::
    PluginResponseInterceptorURLLoaderThrottle(int resource_type,
                                               int frame_tree_node_id)
    : resource_type_(resource_type), frame_tree_node_id_(frame_tree_node_id) {}

PluginResponseInterceptorURLLoaderThrottle::
    ~PluginResponseInterceptorURLLoaderThrottle() = default;

void PluginResponseInterceptorURLLoaderThrottle::WillProcessResponse(
    const GURL& response_url,
    network::mojom::URLResponseHead* response_head,
    bool* defer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (content::download_utils::MustDownload(response_url,
                                            response_head->headers.get(),
                                            response_head->mime_type)) {
    return;
  }

  content::WebContents* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id_);
  if (!web_contents)
    return;

  std::string extension_id = PluginUtils::GetExtensionIdForMimeType(
      web_contents->GetBrowserContext(), response_head->mime_type);

  if (extension_id.empty())
    return;

  std::string view_id = base::GenerateGUID();
  // The string passed down to the original client with the response body.
  std::string payload = view_id;

  network::mojom::URLLoaderPtr dummy_new_loader;
  mojo::MakeRequest(&dummy_new_loader);
  mojo::Remote<network::mojom::URLLoaderClient> new_client;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> new_client_receiver =
      new_client.BindNewPipeAndPassReceiver();

  uint32_t data_pipe_size = 64U;
  // Provide the MimeHandlerView code a chance to override the payload. This is
  // the case where the resource is handled by frame-based MimeHandlerView.
  *defer = extensions::MimeHandlerViewAttachHelper::
      OverrideBodyForInterceptedResponse(
          frame_tree_node_id_, response_url, response_head->mime_type, view_id,
          &payload, &data_pipe_size,
          base::BindOnce(
              &PluginResponseInterceptorURLLoaderThrottle::ResumeLoad,
              weak_factory_.GetWeakPtr()));

  mojo::DataPipe data_pipe(data_pipe_size);
  uint32_t len = static_cast<uint32_t>(payload.size());
  CHECK_EQ(MOJO_RESULT_OK,
           data_pipe.producer_handle->WriteData(
               payload.c_str(), &len, MOJO_WRITE_DATA_FLAG_ALL_OR_NONE));

  new_client->OnStartLoadingResponseBody(std::move(data_pipe.consumer_handle));

  network::URLLoaderCompletionStatus status(net::OK);
  status.decoded_body_length = len;
  new_client->OnComplete(status);

  network::mojom::URLLoaderPtr original_loader;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> original_client;
  delegate_->InterceptResponse(std::move(dummy_new_loader),
                               std::move(new_client_receiver), &original_loader,
                               &original_client);

  // Make a deep copy of URLResponseHead before passing it cross-thread.
  auto deep_copied_response = response_head->Clone();
  if (response_head->headers) {
    deep_copied_response->headers =
        base::MakeRefCounted<net::HttpResponseHeaders>(
            response_head->headers->raw_headers());
  }

  auto transferrable_loader = content::mojom::TransferrableURLLoader::New();
  transferrable_loader->url = GURL(
      extensions::Extension::GetBaseURLFromExtensionId(extension_id).spec() +
      base::GenerateGUID());
  transferrable_loader->url_loader = original_loader.PassInterface();
  transferrable_loader->url_loader_client = std::move(original_client);
  transferrable_loader->head = std::move(deep_copied_response);
  transferrable_loader->head->intercepted_by_plugin = true;

  bool embedded =
      resource_type_ != static_cast<int>(content::ResourceType::kMainFrame);
  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(
          &extensions::StreamsPrivateAPI::SendExecuteMimeTypeHandlerEvent,
          extension_id, view_id, embedded, frame_tree_node_id_,
          -1 /* render_process_id */, -1 /* render_frame_id */,
          std::move(transferrable_loader), response_url));
}

void PluginResponseInterceptorURLLoaderThrottle::ResumeLoad() {
  delegate_->Resume();
}
