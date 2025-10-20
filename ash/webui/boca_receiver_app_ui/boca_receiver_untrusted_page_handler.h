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
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/boca/boca_request.h"
#include "chromeos/ash/components/boca/invalidations/invalidation_service_delegate.h"
#include "chromeos/ash/components/boca/proto/receiver.pb.h"
#include "chromeos/ash/components/boca/retriable_request_sender.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class SkBitmap;

namespace ash::boca {
class InvalidationService;
class SpotlightRemotingClientManager;
enum class CrdConnectionState;
}  // namespace ash::boca

namespace google_apis {
class RequestSender;
enum class HttpRequestMethod;
}  // namespace google_apis

namespace net {
struct NetworkTrafficAnnotationTag;
}  // namespace net

namespace remoting {
class AudioPacket;
}  // namespace remoting

namespace webrtc {
class DesktopFrame;
}  // namespace webrtc

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
  using ConnectionInfoRequestSender =
      boca::RetriableRequestSender<::boca::KioskReceiverConnection>;
  using UpdateReceiverStateRequestSender =
      boca::RetriableRequestSender<::boca::ReceiverConnectionState>;

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
                          std::optional<std::string> receiver_id);

  void UpdateConnection(const std::string& connection_id,
                        ::boca::ReceiverConnectionState request_state);
  void OnUpdateConnectionResponse(
      std::optional<::boca::ReceiverConnectionState> response_state);

  void GetConnectionInfo();
  void OnGetConnectionInfoResponse(
      std::optional<::boca::KioskReceiverConnection> new_connection_info);

  void ProcessStartRequested(
      ::boca::KioskReceiverConnection new_connection_info);
  void ProcessStopRequested(
      const ::boca::KioskReceiverConnection& new_connection_info);
  void MaybeStartConnection(
      ::boca::KioskReceiverConnection new_connection_info);
  void MaybeEndConnection(mojom::ConnectionClosedReason reason);

  void OnCrdSessionEnded();
  void OnCrdFrameReceived(SkBitmap bitmap,
                          std::unique_ptr<webrtc::DesktopFrame>);
  void OnCrdAudioPacketReceived(std::unique_ptr<remoting::AudioPacket> packet);
  void OnCrdConnectionStateUpdated(boca::CrdConnectionState state);

  mojo::Remote<mojom::UntrustedPage> page_;
  const raw_ptr<ReceiverHandlerDelegate> delegate_;
  std::unique_ptr<boca::SpotlightRemotingClientManager> remoting_client_;
  std::unique_ptr<boca::InvalidationService> invalidation_service_;
  std::unique_ptr<google_apis::RequestSender> registration_request_sender_;
  std::optional<std::string> receiver_id_;
  std::unique_ptr<ConnectionInfoRequestSender>
      connection_info_retriable_sender_;
  std::optional<::boca::KioskReceiverConnection> connection_info_;
  std::unique_ptr<UpdateReceiverStateRequestSender>
      update_connection_retriable_sender_;

  base::WeakPtrFactory<BocaReceiverUntrustedPageHandler> weak_ptr_factory_{
      this};
};

}  // namespace ash::boca_receiver

#endif  // ASH_WEBUI_BOCA_RECEIVER_APP_UI_BOCA_RECEIVER_UNTRUSTED_PAGE_HANDLER_H_
