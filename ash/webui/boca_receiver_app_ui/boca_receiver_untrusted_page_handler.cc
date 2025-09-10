// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/boca_receiver_app_ui/boca_receiver_untrusted_page_handler.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "ash/webui/boca_receiver_app_ui/mojom/boca_receiver.mojom.h"
#include "ash/webui/boca_receiver_app_ui/url_constants.h"
#include "base/functional/callback.h"
#include "chromeos/ash/components/boca/boca_request.h"
#include "chromeos/ash/components/boca/invalidations/invalidation_service_impl.h"
#include "chromeos/ash/components/boca/receiver/receiver_handler_delegate.h"
#include "chromeos/ash/components/boca/receiver/register_receiver_request.h"
#include "google_apis/common/base_requests.h"
#include "google_apis/common/request_sender.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace ash::boca_receiver {
namespace {

constexpr std::string_view kRequesterId = "boca-receiver";

}  // namespace

BocaReceiverUntrustedPageHandler::BocaReceiverUntrustedPageHandler(
    mojo::PendingRemote<mojom::UntrustedPage> page,
    ReceiverHandlerDelegate* delegate)
    : page_(std::move(page)), delegate_(delegate) {
  if (!delegate_->IsAppEnabled(kChromeBocaReceiverURL)) {
    page_->OnInitReceiverError();
    return;
  }
  Init();
}

BocaReceiverUntrustedPageHandler::~BocaReceiverUntrustedPageHandler() = default;

void BocaReceiverUntrustedPageHandler::UploadToken(
    const std::string& fcm_token,
    base::OnceCallback<void(bool)> on_token_uploaded_cb) {
  Register(fcm_token, std::move(on_token_uploaded_cb));
}

void BocaReceiverUntrustedPageHandler::OnInvalidationReceived(
    const std::string& payload) {}

void BocaReceiverUntrustedPageHandler::Init() {
  invalidation_service_ = delegate_->CreateInvalidationService(this);
}

std::unique_ptr<google_apis::RequestSender>
BocaReceiverUntrustedPageHandler::SendRequest(
    std::unique_ptr<boca::BocaRequest::Delegate> request_delegate,
    google_apis::HttpRequestMethod request_type,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  auto request_sender =
      delegate_->CreateRequestSender(kRequesterId, traffic_annotation);
  auto request = std::make_unique<boca::BocaRequest>(
      request_sender.get(), request_type, std::move(request_delegate));
  request_sender->StartRequestWithAuthRetry(std::move(request));
  return request_sender;
}

void BocaReceiverUntrustedPageHandler::Register(
    const std::string& fcm_token,
    base::OnceCallback<void(bool)> on_done_cb) {
  auto response_cb =
      base::BindOnce(&BocaReceiverUntrustedPageHandler::OnRegisterResponse,
                     base::Unretained(this), std::move(on_done_cb));
  auto registration_request_delegate =
      std::make_unique<RegisterReceiverRequest>(fcm_token,
                                                std::move(response_cb));
  registration_request_sender_ =
      SendRequest(std::move(registration_request_delegate),
                  google_apis::HttpRequestMethod::kPost,
                  RegisterReceiverRequest::kTrafficAnnotation);
}

void BocaReceiverUntrustedPageHandler::OnRegisterResponse(
    base::OnceCallback<void(bool)> on_done_cb,
    std::optional<std::string> device_id) {
  if (!device_id.has_value()) {
    page_->OnInitReceiverError();
    std::move(on_done_cb).Run(false);
    return;
  }
  mojom::ReceiverInfoPtr receiver_info = mojom::ReceiverInfo::New();
  receiver_info->id = device_id.value();
  page_->OnInitReceiverInfo(std::move(receiver_info));
  std::move(on_done_cb).Run(true);
}

}  // namespace ash::boca_receiver
