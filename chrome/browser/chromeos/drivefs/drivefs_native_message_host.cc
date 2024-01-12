// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drivefs/drivefs_native_message_host.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/drivefs/drivefs_native_message_host_origins.h"
#include "chrome/browser/extensions/api/messaging/native_message_port.h"
#include "chrome/browser/profiles/profile.h"
#include "components/drive/file_errors.h"
#include "extensions/browser/api/messaging/channel_endpoint.h"
#include "extensions/browser/api/messaging/message_service.h"
#include "extensions/browser/api/messaging/native_message_host.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/api/messaging/messaging_endpoint.h"
#include "extensions/common/api/messaging/port_id.h"
#include "extensions/common/mojom/message_port.mojom-shared.h"
#include "extensions/common/permissions/permissions_data.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#endif

namespace drive {

class DriveFsNativeMessageHost : public extensions::NativeMessageHost,
                                 public drivefs::mojom::NativeMessagingPort {
 public:
  // Used when the native messaging session is initiated by the extension.
  explicit DriveFsNativeMessageHost(CreateNativeHostSessionCallback callback)
      : create_native_host_callback_(std::move(callback)) {}

  // Used when the native messaging session is initiated by DriveFS.
  DriveFsNativeMessageHost(
      Profile* profile,
      mojo::PendingReceiver<drivefs::mojom::NativeMessagingPort>
          extension_receiver,
      mojo::PendingRemote<drivefs::mojom::NativeMessagingHost> drivefs_remote)
      :
#if BUILDFLAG(IS_CHROMEOS_LACROS)
        keep_alive_(std::make_unique<ScopedKeepAlive>(
            KeepAliveOrigin::DRIVEFS_NATIVE_MESSAGE_HOST_LACROS,
            KeepAliveRestartOption::ENABLED)),
        profile_keep_alive_(std::make_unique<ScopedProfileKeepAlive>(
            profile,
            ProfileKeepAliveOrigin::kDriveFsNativeMessageHostLacros)),
#endif
        pending_receiver_(std::move(extension_receiver)),
        drivefs_remote_(std::move(drivefs_remote)) {
  }

  DriveFsNativeMessageHost(const DriveFsNativeMessageHost&) = delete;
  DriveFsNativeMessageHost& operator=(const DriveFsNativeMessageHost&) = delete;

  ~DriveFsNativeMessageHost() override = default;

  void OnMessage(const std::string& message) override {
    DCHECK(client_);

    if (drivefs_remote_) {
      drivefs_remote_->HandleMessageFromExtension(message);
    }
  }

  void Start(Client* client) override {
    client_ = client;

    if (!pending_receiver_) {
      // The session was initiated by the extension.
      mojo::PendingRemote<drivefs::mojom::NativeMessagingPort> extension_port;
      pending_receiver_ = extension_port.InitWithNewPipeAndPassReceiver();

      std::move(create_native_host_callback_)
          .Run(drivefs::mojom::ExtensionConnectionParams::New(
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
  void PostMessageToExtension(const std::string& message) override {
    client_->PostMessageFromNativeHost(message);
  }

  void OnDisconnect(uint32_t error, const std::string& reason) {
    client_->CloseChannel(FileErrorToString(static_cast<FileError>(
                              -static_cast<int32_t>(error))) +
                          ": " + reason);
    drivefs_remote_.reset();
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Used to keep lacros alive while connected to DriveFS.
  std::unique_ptr<ScopedKeepAlive> keep_alive_;
  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive_;
#endif

  CreateNativeHostSessionCallback create_native_host_callback_;

  // Used to buffer messages until Start() has been called.
  mojo::PendingReceiver<drivefs::mojom::NativeMessagingPort> pending_receiver_;
  mojo::Receiver<drivefs::mojom::NativeMessagingPort> receiver_{this};
  mojo::Remote<drivefs::mojom::NativeMessagingHost> drivefs_remote_;

  raw_ptr<Client> client_ = nullptr;

  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_ =
      base::SingleThreadTaskRunner::GetCurrentDefault();
};

std::unique_ptr<extensions::NativeMessageHost> CreateDriveFsNativeMessageHost(
    CreateNativeHostSessionCallback callback) {
  return std::make_unique<DriveFsNativeMessageHost>(std::move(callback));
}

std::unique_ptr<extensions::NativeMessageHost>
CreateDriveFsInitiatedNativeMessageHostInternal(
    Profile* profile,
    mojo::PendingReceiver<drivefs::mojom::NativeMessagingPort>
        extension_receiver,
    mojo::PendingRemote<drivefs::mojom::NativeMessagingHost> drivefs_remote) {
  return std::make_unique<DriveFsNativeMessageHost>(
      profile, std::move(extension_receiver), std::move(drivefs_remote));
}

drivefs::mojom::ExtensionConnectionStatus
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
    return drivefs::mojom::ExtensionConnectionStatus::kExtensionNotFound;
  }

  const extensions::PortId port_id(
      base::UnguessableToken::Create(),
      /* port_number= */ 1, /* is_opener= */ true,
      extensions::mojom::SerializationFormat::kJson);
  extensions::MessageService* const message_service =
      extensions::MessageService::Get(profile);
  auto native_message_host = CreateDriveFsInitiatedNativeMessageHostInternal(
      profile, std::move(extension_receiver), std::move(drivefs_remote));
  if (!native_message_host) {
    return drivefs::mojom::ExtensionConnectionStatus::kFeatureNotEnabled;
  }

  auto native_message_port = std::make_unique<extensions::NativeMessagePort>(
      message_service->GetChannelDelegate(), port_id,
      std::move(native_message_host));
  message_service->OpenChannelToExtension(
      extensions::ChannelEndpoint(profile), port_id,
      extensions::MessagingEndpoint::ForNativeApp(
          kDriveFsNativeMessageHostName),
      std::move(native_message_port), extension_id, GURL(),
      extensions::mojom::ChannelType::kNative,
      /* channel name= */ std::string());
  return drivefs::mojom::ExtensionConnectionStatus::kSuccess;
}

}  // namespace drive
