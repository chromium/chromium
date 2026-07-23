// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMANDS_FACTORY_ASH_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMANDS_FACTORY_ASH_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/ash/policy/remote_commands/device_command_screenshot_job.h"
#include "components/policy/core/common/remote_commands/remote_commands_factory.h"

class PrefService;

namespace ash::attestation {
class MachineCertificateUploader;
}  // namespace ash::attestation

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace policy {

class BrowserPolicyConnectorAsh;
class StartCrdSessionJobDelegate;

class DeviceCommandsFactoryAsh : public RemoteCommandsFactory {
 public:
  // `local_state` and `browser_policy_connector_ash` must not be null and must
  // outlive this object.
  // `shared_url_loader_factory` must be non-null.
  DeviceCommandsFactoryAsh(
      PrefService* local_state,
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
      ash::attestation::MachineCertificateUploader* certificate_uploader,
      StartCrdSessionJobDelegate& crd_delegate);

  DeviceCommandsFactoryAsh(const DeviceCommandsFactoryAsh&) = delete;
  DeviceCommandsFactoryAsh& operator=(const DeviceCommandsFactoryAsh&) = delete;

  ~DeviceCommandsFactoryAsh() override;

  // RemoteCommandsFactory:
  std::unique_ptr<RemoteCommandJob> BuildJobForType(
      enterprise_management::RemoteCommand_Type type,
      RemoteCommandsService* service) override;

  static void set_commands_for_testing(bool device_commands_test);

 private:
  // TODO(b/269432279): Consider removing when test uses a local upload server
  static bool device_commands_test_;

  const raw_ref<PrefService> local_state_;
  const scoped_refptr<network::SharedURLLoaderFactory>
      shared_url_loader_factory_;
  raw_ptr<ash::attestation::MachineCertificateUploader>
      machine_certificate_uploader_;
  raw_ref<StartCrdSessionJobDelegate> crd_delegate_;

  std::unique_ptr<DeviceCommandScreenshotJob::Delegate>
  CreateScreenshotDelegate(
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
      BrowserPolicyConnectorAsh* browser_policy_connector_ash);
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMANDS_FACTORY_ASH_H_
