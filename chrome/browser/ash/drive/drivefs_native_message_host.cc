// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/drive/drivefs_native_message_host.h"

#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/extensions/api/messaging/native_message_port.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/drive/file_errors.h"
#include "extensions/browser/api/messaging/channel_endpoint.h"
#include "extensions/browser/api/messaging/message_service.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/api/messaging/messaging_endpoint.h"
#include "extensions/common/api/messaging/port_id.h"
#include "extensions/common/api/messaging/serialization_format.h"
#include "extensions/common/permissions/permissions_data.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace drive {

const char kDriveFsNativeMessageHostName[] = "com.google.drive.nativeproxy";

const char* const kDriveFsNativeMessageHostOrigins[] = {
    "chrome-extension://lmjegmlicamnimmfhcmpkclmigmmcbeh/",
};

constexpr size_t kDriveFsNativeMessageHostOriginsSize =
    std::size(kDriveFsNativeMessageHostOrigins);

class DriveFsNativeMessageHost : public extensions::NativeMessageHost,
                                 public drivefs::mojom::NativeMessagingPort {
 public:
  // Used when the native messaging session is initiated by the extension.
  explicit DriveFsNativeMessageHost(Profile* profile)
      : drive_service_(DriveIntegrationServiceFactory::GetForProfile(profile)) {
  }

  // Used when the native messaging session is initiated by DriveFS.
  DriveFsNativeMessageHost(
      mojo::PendingReceiver<drivefs::mojom::NativeMessagingPort>
          extension_receiver,
      mojo::PendingRemote<drivefs::mojom::NativeMessagingHost> drivefs_remote)
      : pending_receiver_(std::move(extension_receiver)),
        drivefs_remote_(std::move(drivefs_remote)) {
    DCHECK(UseBidirectionalNativeMessaging());
  }

  explicit DriveFsNativeMessageHost(
      drivefs::mojom::DriveFs* drivefs_for_testing)
      : drivefs_for_testing_(drivefs_for_testing) {}

  DriveFsNativeMessageHost(const DriveFsNativeMessageHost&) = delete;
  DriveFsNativeMessageHost& operator=(const DriveFsNativeMessageHost&) = delete;

  ~DriveFsNativeMessageHost() override = default;

  void OnMessage(const std::string& message) override {
    DCHECK(client_);

    if (UseBidirectionalNativeMessaging()) {
      if (drivefs_remote_) {
        drivefs_remote_->HandleMessageFromExtension(message);
      }
    } else {
      if (!drive_service_ || !drive_service_->GetDriveFsInterface()) {
        OnDriveFsResponse(FILE_ERROR_SERVICE_UNAVAILABLE, "");
        return;
      }

      drive_service_->GetDriveFsInterface()->SendNativeMessageRequest(
          message, base::BindOnce(&DriveFsNativeMessageHost::OnDriveFsResponse,
                                  weak_ptr_factory_.GetWeakPtr()));
    }
  }

  void Start(Client* client) override {
    client_ = client;

    if (!UseBidirectionalNativeMessaging()) {
      return;
    }

    if (!pending_receiver_) {
      // The session was initiated by the extension.
      mojo::PendingRemote<drivefs::mojom::NativeMessagingPort> extension_port;
      pending_receiver_ = extension_port.InitWithNewPipeAndPassReceiver();

      drivefs::mojom::DriveFs* drivefs;
      if (drivefs_for_testing_) {
        drivefs = drivefs_for_testing_;
      } else if (!drive_service_ || !drive_service_->GetDriveFsInterface()) {
        OnDriveFsResponse(FILE_ERROR_SERVICE_UNAVAILABLE, "");
        return;
      } else {
        drivefs = drive_service_->GetDriveFsInterface();
      }

      drivefs->CreateNativeHostSession(
          drivefs::mojom::ExtensionConnectionParams::New(
              GURL(kDriveFsNativeMessageHostOrigins[0]).host()),
          drivefs_remote_.BindNewPipeAndPassReceiver(),
          std::move(extension_port));
    }
    receiver_.Bind(std::move(std::move(pending_receiver_)));
    receiver_.set_disconnect_with_reason_handler(base::BindOnce(
        &DriveFsNativeMessageHost::OnDisconnect, base::Unretained(this)));
  }

  scoped_refptr<base::SingleThreadTaskRunner> task_runner() const override {
    return task_runner_;
  }

 private:
  void OnDriveFsResponse(FileError error, const std::string& response) {
    if (error == FILE_ERROR_OK) {
      client_->PostMessageFromNativeHost(response);
    } else {
      LOG(WARNING) << "DriveFS returned error " << FileErrorToString(error);
      client_->CloseChannel(FileErrorToString(error));
    }
  }

  void PostMessageToExtension(const std::string& message) override {
    client_->PostMessageFromNativeHost(message);
  }

  void OnDisconnect(uint32_t error, const std::string& reason) {
    client_->CloseChannel(FileErrorToString(static_cast<FileError>(
                              -static_cast<int32_t>(error))) +
                          ": " + reason);
    drivefs_remote_.reset();
  }

  bool UseBidirectionalNativeMessaging() {
    return base::FeatureList::IsEnabled(
        ash::features::kDriveFsBidirectionalNativeMessaging);
  }

  DriveIntegrationService* drive_service_ = nullptr;
  drivefs::mojom::DriveFs* drivefs_for_testing_ = nullptr;

  // Used to buffer messages until Start() has been called.
  mojo::PendingReceiver<drivefs::mojom::NativeMessagingPort> pending_receiver_;
  mojo::Receiver<drivefs::mojom::NativeMessagingPort> receiver_{this};
  mojo::Remote<drivefs::mojom::NativeMessagingHost> drivefs_remote_;

  Client* client_ = nullptr;

  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_ =
      base::SingleThreadTaskRunner::GetCurrentDefault();

  base::WeakPtrFactory<DriveFsNativeMessageHost> weak_ptr_factory_{this};
};

std::unique_ptr<extensions::NativeMessageHost> CreateDriveFsNativeMessageHost(
    content::BrowserContext* browser_context) {
  return std::make_unique<DriveFsNativeMessageHost>(
      Profile::FromBrowserContext(browser_context));
}

std::unique_ptr<extensions::NativeMessageHost>
CreateDriveFsInitiatedNativeMessageHost(
    mojo::PendingReceiver<drivefs::mojom::NativeMessagingPort>
        extension_receiver,
    mojo::PendingRemote<drivefs::mojom::NativeMessagingHost> drivefs_remote) {
  if (!base::FeatureList::IsEnabled(
          ash::features::kDriveFsBidirectionalNativeMessaging)) {
    return nullptr;
  }
  return std::make_unique<DriveFsNativeMessageHost>(
      std::move(extension_receiver), std::move(drivefs_remote));
}

std::unique_ptr<extensions::NativeMessageHost>
CreateDriveFsNativeMessageHostForTesting(
    drivefs::mojom::DriveFs* drivefs_for_testing) {
  return std::make_unique<DriveFsNativeMessageHost>(drivefs_for_testing);
}

drivefs::mojom::DriveFsDelegate::ExtensionConnectionStatus
ConnectToDriveFsNativeMessageExtension(
    Profile* profile,
    const std::string& extension_id,
    mojo::PendingReceiver<drivefs::mojom::NativeMessagingPort>
        extension_receiver,
    mojo::PendingRemote<drivefs::mojom::NativeMessagingHost> drivefs_remote) {
  auto* extension =
      extensions::ExtensionRegistry::Get(profile)->enabled_extensions().GetByID(
          extension_id);
  if (!extension ||
      !extension->permissions_data()->active_permissions().HasAPIPermission(
          "nativeMessaging") ||
      !extensions::EventRouter::Get(profile)->ExtensionHasEventListener(
          extension_id, "runtime.onConnectNative")) {
    return drivefs::mojom::DriveFsDelegate::ExtensionConnectionStatus::
        kExtensionNotFound;
  }

  const extensions::PortId port_id(base::UnguessableToken::Create(),
                                   /* port_number= */ 1, /* is_opener= */ true,
                                   extensions::SerializationFormat::kJson);
  extensions::MessageService* const message_service =
      extensions::MessageService::Get(profile);
  auto native_message_host = CreateDriveFsInitiatedNativeMessageHost(
      std::move(extension_receiver), std::move(drivefs_remote));
  if (!native_message_host) {
    return drivefs::mojom::DriveFsDelegate::ExtensionConnectionStatus::
        kFeatureNotEnabled;
  }

  auto native_message_port = std::make_unique<extensions::NativeMessagePort>(
      message_service->GetChannelDelegate(), port_id,
      std::move(native_message_host));
  message_service->OpenChannelToExtension(
      extensions::ChannelEndpoint(profile), port_id,
      extensions::MessagingEndpoint::ForNativeApp(
          kDriveFsNativeMessageHostName),
      std::move(native_message_port), extension_id, GURL(),
      /* channel name= */ std::string());
  return drivefs::mojom::DriveFsDelegate::ExtensionConnectionStatus::kSuccess;
}

}  // namespace drive
