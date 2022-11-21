// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/wilco_dtc_supportd/wilco_dtc_supportd_bridge.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/process/process_handle.h"
#include "base/strings/string_piece.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/wilco_dtc_supportd/mojo_utils.h"
#include "chrome/browser/ash/wilco_dtc_supportd/wilco_dtc_supportd_client.h"
#include "chrome/browser/ash/wilco_dtc_supportd/wilco_dtc_supportd_messaging.h"
#include "chrome/browser/ash/wilco_dtc_supportd/wilco_dtc_supportd_network_context.h"
#include "chrome/browser/ash/wilco_dtc_supportd/wilco_dtc_supportd_notification_controller.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "third_party/cros_system_api/dbus/wilco_dtc_supportd/dbus-constants.h"

namespace ash {

namespace {

using chromeos::wilco_dtc_supportd::mojom::WilcoDtcSupportdEvent;

// Interval used between successive connection attempts to the
// wilco_dtc_supportd. This is a safety measure for avoiding busy loops when the
// wilco_dtc_supportd is dysfunctional.
constexpr base::TimeDelta kConnectionAttemptInterval = base::Seconds(1);
// The maximum number of consecutive connection attempts to the
// wilco_dtc_supportd before giving up. This is to prevent wasting system
// resources on hopeless attempts to connect in cases when the
// wilco_dtc_supportd is dysfunctional.
constexpr int kMaxConnectionAttemptCount = 10;

WilcoDtcSupportdBridge* g_wilco_dtc_supportd_bridge_instance = nullptr;

// Real implementation of the WilcoDtcSupportdBridge delegate.
class WilcoDtcSupportdBridgeDelegateImpl final
    : public WilcoDtcSupportdBridge::Delegate {
 public:
  WilcoDtcSupportdBridgeDelegateImpl();

  WilcoDtcSupportdBridgeDelegateImpl(
      const WilcoDtcSupportdBridgeDelegateImpl&) = delete;
  WilcoDtcSupportdBridgeDelegateImpl& operator=(
      const WilcoDtcSupportdBridgeDelegateImpl&) = delete;

  ~WilcoDtcSupportdBridgeDelegateImpl() override;

  // Delegate overrides:
  void CreateWilcoDtcSupportdServiceFactoryMojoInvitation(
      mojo::Remote<
          chromeos::wilco_dtc_supportd::mojom::WilcoDtcSupportdServiceFactory>*
          wilco_dtc_supportd_service_factory_mojo_remote,
      base::ScopedFD* remote_endpoint_fd) override;
};

WilcoDtcSupportdBridgeDelegateImpl::WilcoDtcSupportdBridgeDelegateImpl() =
    default;

WilcoDtcSupportdBridgeDelegateImpl::~WilcoDtcSupportdBridgeDelegateImpl() =
    default;

void WilcoDtcSupportdBridgeDelegateImpl::
    CreateWilcoDtcSupportdServiceFactoryMojoInvitation(
        mojo::Remote<chromeos::wilco_dtc_supportd::mojom::
                         WilcoDtcSupportdServiceFactory>*
            wilco_dtc_supportd_service_factory_mojo_remote,
        base::ScopedFD* remote_endpoint_fd) {
  mojo::OutgoingInvitation invitation;
  mojo::PlatformChannel channel;
  mojo::ScopedMessagePipeHandle server_pipe = invitation.AttachMessagePipe(
      diagnostics::kWilcoDtcSupportdMojoConnectionChannelToken);
  mojo::OutgoingInvitation::Send(std::move(invitation),
                                 base::kNullProcessHandle,
                                 channel.TakeLocalEndpoint());
  wilco_dtc_supportd_service_factory_mojo_remote->Bind(
      mojo::PendingRemote<
          chromeos::wilco_dtc_supportd::mojom::WilcoDtcSupportdServiceFactory>(
          std::move(server_pipe), 0 /* version */));
  *remote_endpoint_fd =
      channel.TakeRemoteEndpoint().TakePlatformHandle().TakeFD();
}

}  // namespace

WilcoDtcSupportdBridge::Delegate::~Delegate() = default;

// static
WilcoDtcSupportdBridge* WilcoDtcSupportdBridge::Get() {
  return g_wilco_dtc_supportd_bridge_instance;
}

// static
base::TimeDelta
WilcoDtcSupportdBridge::connection_attempt_interval_for_testing() {
  return kConnectionAttemptInterval;
}

// static
int WilcoDtcSupportdBridge::max_connection_attempt_count_for_testing() {
  return kMaxConnectionAttemptCount;
}

WilcoDtcSupportdBridge::WilcoDtcSupportdBridge(
    std::unique_ptr<WilcoDtcSupportdNetworkContext> network_context)
    : WilcoDtcSupportdBridge(
          std::make_unique<WilcoDtcSupportdBridgeDelegateImpl>(),
          std::move(network_context),
          std::make_unique<WilcoDtcSupportdNotificationController>()) {}

WilcoDtcSupportdBridge::WilcoDtcSupportdBridge(
    std::unique_ptr<Delegate> delegate,
    std::unique_ptr<WilcoDtcSupportdNetworkContext> network_context,
    std::unique_ptr<WilcoDtcSupportdNotificationController>
        notification_controller)
    : delegate_(std::move(delegate)),
      web_request_service_(std::move(network_context)),
      notification_controller_(std::move(notification_controller)) {
  DCHECK(delegate_);
  DCHECK(notification_controller_);
  DCHECK(!g_wilco_dtc_supportd_bridge_instance);
  g_wilco_dtc_supportd_bridge_instance = this;
  WaitForDBusService();
}

WilcoDtcSupportdBridge::~WilcoDtcSupportdBridge() {
  DCHECK_EQ(g_wilco_dtc_supportd_bridge_instance, this);
  g_wilco_dtc_supportd_bridge_instance = nullptr;
}

void WilcoDtcSupportdBridge::SetConfigurationData(const std::string* data) {
  configuration_data_ = data;
}

const std::string& WilcoDtcSupportdBridge::GetConfigurationDataForTesting() {
  return configuration_data_ ? *configuration_data_ : base::EmptyString();
}

void WilcoDtcSupportdBridge::WaitForDBusService() {
  if (connection_attempt_ >= kMaxConnectionAttemptCount) {
    DLOG(WARNING)
        << "Stopping attempts to connect to wilco_dtc_supportd - too many "
           "unsuccessful attempts in a row";
    return;
  }
  ++connection_attempt_;

  // Cancel any tasks previously created from WaitForDBusService() or
  // ScheduleWaitingForDBusService().
  dbus_waiting_weak_ptr_factory_.InvalidateWeakPtrs();

  // Prefix namespace ash to explicitly specify the one in the namespace,
  // rather than the parent class.
  ash::WilcoDtcSupportdClient::Get()->WaitForServiceToBeAvailable(
      base::BindOnce(&WilcoDtcSupportdBridge::OnWaitedForDBusService,
                     dbus_waiting_weak_ptr_factory_.GetWeakPtr()));
}

void WilcoDtcSupportdBridge::ScheduleWaitingForDBusService() {
  // Cancel any tasks previously created from WaitForDBusService() or
  // ScheduleWaitingForDBusService().
  dbus_waiting_weak_ptr_factory_.InvalidateWeakPtrs();

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&WilcoDtcSupportdBridge::WaitForDBusService,
                     dbus_waiting_weak_ptr_factory_.GetWeakPtr()),
      kConnectionAttemptInterval);
}

void WilcoDtcSupportdBridge::OnWaitedForDBusService(bool service_is_available) {
  if (!service_is_available) {
    DLOG(WARNING) << "The wilco_dtc_supportd D-Bus service is unavailable";
    return;
  }

  // Cancel any tasks previously created from WaitForDBusService() or
  // ScheduleWaitingForDBusService().
  dbus_waiting_weak_ptr_factory_.InvalidateWeakPtrs();

  BootstrapMojoConnection();
}

void WilcoDtcSupportdBridge::BootstrapMojoConnection() {
  DCHECK(!wilco_dtc_supportd_service_factory_mojo_remote_);

  // Create a Mojo message pipe and attach
  // |wilco_dtc_supportd_service_factory_mojo_remote_| to its local endpoint.
  base::ScopedFD remote_endpoint_fd;
  delegate_->CreateWilcoDtcSupportdServiceFactoryMojoInvitation(
      &wilco_dtc_supportd_service_factory_mojo_remote_, &remote_endpoint_fd);
  DCHECK(wilco_dtc_supportd_service_factory_mojo_remote_);
  DCHECK(remote_endpoint_fd.is_valid());
  wilco_dtc_supportd_service_factory_mojo_remote_.set_disconnect_handler(
      base::BindOnce(&WilcoDtcSupportdBridge::OnMojoConnectionError,
                     weak_ptr_factory_.GetWeakPtr()));

  // Queue a call that would establish full-duplex Mojo communication with the
  // wilco_dtc_supportd daemon by sending an interface pointer to the self
  // instance.
  mojo_self_receiver_.reset();
  mojo::PendingRemote<
      chromeos::wilco_dtc_supportd::mojom::WilcoDtcSupportdClient>
      self_proxy;
  mojo_self_receiver_.Bind(self_proxy.InitWithNewPipeAndPassReceiver());
  wilco_dtc_supportd_service_factory_mojo_remote_->GetService(
      wilco_dtc_supportd_service_mojo_remote_.BindNewPipeAndPassReceiver(),
      std::move(self_proxy),
      base::BindOnce(&WilcoDtcSupportdBridge::OnMojoGetServiceCompleted,
                     weak_ptr_factory_.GetWeakPtr()));

  // Send the file descriptor with the Mojo message pipe's remote endpoint to
  // the wilco_dtc_supportd daemon via the D-Bus. Also, prefix namespace ash to
  // explicitly specify the one in the namespace, rather than the parent class.
  ash::WilcoDtcSupportdClient::Get()->BootstrapMojoConnection(
      std::move(remote_endpoint_fd),
      base::BindOnce(&WilcoDtcSupportdBridge::OnBootstrappedMojoConnection,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WilcoDtcSupportdBridge::OnBootstrappedMojoConnection(bool success) {
  if (success)
    return;
  DLOG(ERROR) << "Failed to establish Mojo connection to wilco_dtc_supportd";
  wilco_dtc_supportd_service_factory_mojo_remote_.reset();
  wilco_dtc_supportd_service_mojo_remote_.reset();
  ScheduleWaitingForDBusService();
}

void WilcoDtcSupportdBridge::OnMojoGetServiceCompleted() {
  DCHECK(wilco_dtc_supportd_service_mojo_remote_);
  DVLOG(0) << "Established Mojo communication with wilco_dtc_supportd";
  // Reset the current connection attempt counter, since a successful
  // initialization of Mojo communication has completed.
  connection_attempt_ = 0;
}

void WilcoDtcSupportdBridge::OnMojoConnectionError() {
  DLOG(WARNING)
      << "Mojo connection to the wilco_dtc_supportd daemon got shut down";
  wilco_dtc_supportd_service_factory_mojo_remote_.reset();
  wilco_dtc_supportd_service_mojo_remote_.reset();
  ScheduleWaitingForDBusService();
}

void WilcoDtcSupportdBridge::PerformWebRequest(
    chromeos::wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestHttpMethod
        http_method,
    mojo::ScopedHandle url,
    std::vector<mojo::ScopedHandle> headers,
    mojo::ScopedHandle request_body,
    PerformWebRequestCallback callback) {
  // Extract a GURL value from a ScopedHandle.
  GURL gurl;
  if (url.is_valid()) {
    base::ReadOnlySharedMemoryMapping shared_memory;
    gurl = GURL(MojoUtils::GetStringPieceFromMojoHandle(std::move(url),
                                                        &shared_memory));
    if (!shared_memory.IsValid()) {
      LOG(ERROR) << "Failed to read data from mojo handle";
      std::move(callback).Run(
          chromeos::wilco_dtc_supportd::mojom::
              WilcoDtcSupportdWebRequestStatus::kNetworkError,
          0 /* http_status */, mojo::ScopedHandle() /* response_body */);
      return;
    }
  }

  // Extract headers from ScopedHandle's.
  std::vector<base::StringPiece> header_contents;
  std::vector<base::ReadOnlySharedMemoryMapping> shared_memories;
  for (auto& header : headers) {
    if (!header.is_valid()) {
      header_contents.push_back("");
      continue;
    }
    shared_memories.emplace_back();
    header_contents.push_back(MojoUtils::GetStringPieceFromMojoHandle(
        std::move(header), &shared_memories.back()));
    if (!shared_memories.back().IsValid()) {
      LOG(ERROR) << "Failed to read data from mojo handle";
      std::move(callback).Run(
          chromeos::wilco_dtc_supportd::mojom::
              WilcoDtcSupportdWebRequestStatus::kNetworkError,
          0 /* http_status */, mojo::ScopedHandle() /* response_body */);
      return;
    }
  }

  // Extract a string value from a ScopedHandle.
  std::string request_body_content;
  if (request_body.is_valid()) {
    base::ReadOnlySharedMemoryMapping shared_memory;
    request_body_content = std::string(MojoUtils::GetStringPieceFromMojoHandle(
        std::move(request_body), &shared_memory));
    if (!shared_memory.IsValid()) {
      LOG(ERROR) << "Failed to read data from mojo handle";
      std::move(callback).Run(
          chromeos::wilco_dtc_supportd::mojom::
              WilcoDtcSupportdWebRequestStatus::kNetworkError,
          0 /* http_status */, mojo::ScopedHandle() /* response_body */);
      return;
    }
  }

  web_request_service_.PerformRequest(
      http_method, std::move(gurl), std::move(header_contents),
      std::move(request_body_content), std::move(callback));
}

void WilcoDtcSupportdBridge::SendWilcoDtcMessageToUi(
    mojo::ScopedHandle json_message,
    SendWilcoDtcMessageToUiCallback callback) {
  // Extract the string value of the received message.
  DCHECK(json_message);
  base::ReadOnlySharedMemoryMapping json_message_shared_memory;
  base::StringPiece json_message_string =
      MojoUtils::GetStringPieceFromMojoHandle(std::move(json_message),
                                              &json_message_shared_memory);
  if (json_message_string.empty()) {
    LOG(ERROR) << "Failed to read data from mojo handle";
    std::move(callback).Run(mojo::ScopedHandle() /* response_json_message */);
    return;
  }

  DeliverWilcoDtcSupportdUiMessageToExtensions(
      std::string(json_message_string),
      base::BindOnce(
          [](SendWilcoDtcMessageToUiCallback callback,
             const std::string& response) {
            mojo::ScopedHandle response_mojo_handle;
            if (!response.empty()) {
              response_mojo_handle =
                  MojoUtils::CreateReadOnlySharedMemoryMojoHandle(response);
              if (!response_mojo_handle)
                LOG(ERROR) << "Failed to create mojo handle for string";
            }
            std::move(callback).Run(std::move(response_mojo_handle));
          },
          std::move(callback)));
}

void WilcoDtcSupportdBridge::GetConfigurationData(
    GetConfigurationDataCallback callback) {
  std::move(callback).Run(configuration_data_ ? *configuration_data_
                                              : std::string());
}

void WilcoDtcSupportdBridge::HandleEvent(WilcoDtcSupportdEvent event) {
  switch (event) {
    case WilcoDtcSupportdEvent::kBatteryAuth:
      notification_controller_->ShowBatteryAuthNotification();
      return;
    case WilcoDtcSupportdEvent::kNonWilcoCharger:
      notification_controller_->ShowNonWilcoChargerNotification();
      return;
    case WilcoDtcSupportdEvent::kIncompatibleDock:
      notification_controller_->ShowIncompatibleDockNotification();
      return;
    case WilcoDtcSupportdEvent::kDockError:
      notification_controller_->ShowDockErrorNotification();
      return;
    case WilcoDtcSupportdEvent::kDockDisplay:
      notification_controller_->ShowDockDisplayNotification();
      return;
    case WilcoDtcSupportdEvent::kDockThunderbolt:
      notification_controller_->ShowDockThunderboltNotification();
      return;
    case WilcoDtcSupportdEvent::kLowPowerCharger:
      notification_controller_->ShowLowPowerChargerNotification();
      return;
    case WilcoDtcSupportdEvent::kUnmappedEnumField:
      LOG(ERROR) << "Get unrecognized event: " << event;
      return;
  }
}

void WilcoDtcSupportdBridge::GetCrosHealthdDiagnosticsService(
    mojo::PendingReceiver<cros_healthd::mojom::CrosHealthdDiagnosticsService>
        service) {
  cros_healthd::ServiceConnection::GetInstance()->BindDiagnosticsService(
      std::move(service));
}

void WilcoDtcSupportdBridge::GetCrosHealthdProbeService(
    mojo::PendingReceiver<cros_healthd::mojom::CrosHealthdProbeService>
        service) {
  cros_healthd::ServiceConnection::GetInstance()->BindProbeService(
      std::move(service));
}

}  // namespace ash
