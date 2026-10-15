// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_extension_service_worker_handler.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_web_contents_user_data.h"
#include "chrome/browser/profiles/profile.h"
#include "components/contextual_tasks/public/features.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "third_party/lens_server_proto/aim_communication.pb.h"
#include "third_party/lens_server_proto/search_communication.pb.h"

namespace contextual_tasks {

namespace {

constexpr size_t kMaxSearchMessageBytes = 1024 * 1024;

}  // namespace

ContextualTasksExtensionServiceWorkerHandler::
    ContextualTasksExtensionServiceWorkerHandler(Profile& profile)
    : profile_(profile) {}

ContextualTasksExtensionServiceWorkerHandler::
    ~ContextualTasksExtensionServiceWorkerHandler() = default;

void ContextualTasksExtensionServiceWorkerHandler::BindFactory(
    mojo::PendingReceiver<mojom::ExtensionPageHandlerFactory> receiver) {
  factory_receiver_.reset();
  factory_receiver_.Bind(std::move(receiver));
}

void ContextualTasksExtensionServiceWorkerHandler::Clear() {
  factory_receiver_.reset();
  handler_receiver_.reset();
  page_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void ContextualTasksExtensionServiceWorkerHandler::CreateExtensionPageHandler(
    mojo::PendingRemote<mojom::ExtensionPage> page,
    mojo::PendingReceiver<mojom::ExtensionPageHandler> receiver) {
  page_.reset();
  page_.Bind(std::move(page));
  handler_receiver_.reset();
  handler_receiver_.Bind(std::move(receiver));
}

void ContextualTasksExtensionServiceWorkerHandler::GetHandshakeMessage(
    const std::optional<std::string>& document_id,
    GetHandshakeMessageCallback callback) {
  if (!base::FeatureList::IsEnabled(
          contextual_tasks::kContextualTasksRearchitecture) ||
      !document_id.has_value() || document_id->empty()) {
    std::move(callback).Run(
        mojo_base::ProtoWrapper(lens::ClientToAimMessage()));
    return;
  }

  content::RenderFrameHost* rfh =
      GetValidatedMainFrameForDocumentId(*document_id);
  if (!rfh) {
    std::move(callback).Run(
        mojo_base::ProtoWrapper(lens::ClientToAimMessage()));
    return;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents) {
    std::move(callback).Run(
        mojo_base::ProtoWrapper(lens::ClientToAimMessage()));
    return;
  }

  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents);
  user_data->OnDocumentConnected(
      rfh,
      base::BindRepeating(
          &ContextualTasksExtensionServiceWorkerHandler::DispatchSearchMessage,
          weak_ptr_factory_.GetWeakPtr(), *document_id));

  std::move(callback).Run(mojo_base::ProtoWrapper(GetHandshakeMessageProto()));
}

void ContextualTasksExtensionServiceWorkerHandler::OnWebviewMessage(
    const std::vector<uint8_t>& message,
    const std::optional<std::string>& document_id) {
  if (!base::FeatureList::IsEnabled(
          contextual_tasks::kContextualTasksRearchitecture) ||
      !document_id.has_value() || document_id->empty()) {
    return;
  }

  if (message.empty() || message.size() > kMaxSearchMessageBytes) {
    return;
  }

  content::RenderFrameHost* rfh =
      GetValidatedMainFrameForDocumentId(*document_id);
  if (!rfh) {
    return;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents) {
    return;
  }

  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents);
  if (!user_data->IsServiceWorkerPortConnected()) {
    user_data->OnDocumentConnected(
        rfh, base::BindRepeating(&ContextualTasksExtensionServiceWorkerHandler::
                                     DispatchSearchMessage,
                                 weak_ptr_factory_.GetWeakPtr(), *document_id));
  }
  user_data->OnSearchMessageReceived(message);
}

void ContextualTasksExtensionServiceWorkerHandler::SetTaskId(
    const base::Uuid& uuid) {}

void ContextualTasksExtensionServiceWorkerHandler::GetLensCropPreview(
    GetLensCropPreviewCallback callback) {
  std::move(callback).Run(std::nullopt);
}

void ContextualTasksExtensionServiceWorkerHandler::RemoveLensCrop() {}

content::RenderFrameHost* ContextualTasksExtensionServiceWorkerHandler::
    GetValidatedMainFrameForDocumentId(
        const std::string& document_id_str) const {
  extensions::ExtensionApiFrameIdMap::DocumentId document_id =
      extensions::ExtensionApiFrameIdMap::DocumentIdFromString(document_id_str);
  if (!document_id) {
    return nullptr;
  }
  content::RenderFrameHost* rfh =
      extensions::ExtensionApiFrameIdMap::Get()->GetRenderFrameHostByDocumentId(
          document_id);
  if (!rfh || !rfh->IsInPrimaryMainFrame()) {
    return nullptr;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents || web_contents->GetBrowserContext() != &profile_.get()) {
    return nullptr;
  }
  return rfh;
}

void ContextualTasksExtensionServiceWorkerHandler::DispatchSearchMessage(
    const std::string& document_id,
    const std::vector<uint8_t>& message) {
  if (!page_) {
    return;
  }
  lens::ClientToSearchMessage proto;
  if (!proto.ParseFromArray(message.data(), message.size())) {
    return;
  }
  page_->PostSearchMessage(mojo_base::ProtoWrapper(proto), document_id);
}

}  // namespace contextual_tasks
