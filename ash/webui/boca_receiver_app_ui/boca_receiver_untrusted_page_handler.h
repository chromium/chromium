// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_BOCA_RECEIVER_APP_UI_BOCA_RECEIVER_UNTRUSTED_PAGE_HANDLER_H_
#define ASH_WEBUI_BOCA_RECEIVER_APP_UI_BOCA_RECEIVER_UNTRUSTED_PAGE_HANDLER_H_

#include <memory>
#include <optional>
#include <string>

#include "ash/webui/boca_receiver_app_ui/mojom/boca_receiver.mojom.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/boca/boca_request.h"
#include "chromeos/ash/components/boca/invalidations/invalidation_service_delegate.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::boca {
class InvalidationServiceImpl;
}  // namespace ash::boca

namespace google_apis {
class RequestSender;
enum class HttpRequestMethod;
}  // namespace google_apis

namespace net {
struct NetworkTrafficAnnotationTag;
}  // namespace net

namespace ash::boca_receiver {

class ReceiverHandlerDelegate;

class BocaReceiverUntrustedPageHandler
    : public boca::InvalidationServiceDelegate {
 public:
  BocaReceiverUntrustedPageHandler(
      mojo::PendingRemote<mojom::UntrustedPage> page,
      ReceiverHandlerDelegate* delegate);

  BocaReceiverUntrustedPageHandler(const BocaReceiverUntrustedPageHandler&) =
      delete;
  BocaReceiverUntrustedPageHandler& operator=(
      const BocaReceiverUntrustedPageHandler&) = delete;

  ~BocaReceiverUntrustedPageHandler() override;

 private:
  // boca::InvalidationServiceDelegate:
  void UploadToken(
      const std::string& fcm_token,
      base::OnceCallback<void(bool)> on_token_uploaded_cb) override;
  void OnInvalidationReceived(const std::string& payload) override;

  void Init();

  [[nodiscard]] std::unique_ptr<google_apis::RequestSender> SendRequest(
      std::unique_ptr<boca::BocaRequest::Delegate> request_delegate,
      const net::NetworkTrafficAnnotationTag& traffic_annotation);

  void Register(const std::string& fcm_token,
                base::OnceCallback<void(bool)> on_done_cb);
  void OnRegisterResponse(base::OnceCallback<void(bool)> on_done_cb,
                          std::optional<std::string> device_id);

  mojo::Remote<mojom::UntrustedPage> page_;
  raw_ptr<ReceiverHandlerDelegate> delegate_;
  std::unique_ptr<boca::InvalidationServiceImpl> invalidation_service_;
  std::unique_ptr<google_apis::RequestSender> registration_request_sender_;
};

}  // namespace ash::boca_receiver

#endif  // ASH_WEBUI_BOCA_RECEIVER_APP_UI_BOCA_RECEIVER_UNTRUSTED_PAGE_HANDLER_H_
