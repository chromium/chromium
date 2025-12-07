// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/key_rotation_command_factory.h"

#include <memory>
#include <optional>

#include "base/no_destructor.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_features.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/key_rotation_command.h"
#include "components/enterprise/client_certificates/core/browser_cloud_management_delegate.h"
#include "components/enterprise/client_certificates/core/dm_server_client.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/win_key_rotation_command.h"
#elif BUILDFLAG(IS_LINUX)
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/linux_key_rotation_command.h"
#elif BUILDFLAG(IS_MAC)
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/mac_key_rotation_command.h"
#endif  // BUILDFLAG(IS_WIN)

namespace enterprise_connectors {

namespace {

std::optional<KeyRotationCommandFactory*>& GetTestInstanceStorage() {
  static std::optional<KeyRotationCommandFactory*> storage;
  return storage;
}

}  // namespace

KeyRotationCommandFactory::~KeyRotationCommandFactory() = default;

// static
KeyRotationCommandFactory* KeyRotationCommandFactory::GetInstance() {
  auto test_instance = GetTestInstanceStorage();
  if (test_instance.has_value() && test_instance.value()) {
    return test_instance.value();
  }
  static base::NoDestructor<KeyRotationCommandFactory> instance;
  return instance.get();
}

std::unique_ptr<KeyRotationCommand> KeyRotationCommandFactory::CreateCommand(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    policy::DeviceManagementService* device_management_service) {
#if BUILDFLAG(IS_WIN)
  return std::make_unique<WinKeyRotationCommand>();
#elif BUILDFLAG(IS_LINUX)
  return std::make_unique<LinuxKeyRotationCommand>(url_loader_factory);
#elif BUILDFLAG(IS_MAC)
  if (IsDTCKeyRotationUploadedBySharedAPI()) {
    auto cloud_delegate = std::make_unique<
        enterprise_attestation::BrowserCloudManagementDelegate>(
        enterprise_attestation::DMServerClient::Create(
            device_management_service, url_loader_factory));
    return std::make_unique<MacKeyRotationCommand>(std::move(cloud_delegate));
  }
  return std::make_unique<MacKeyRotationCommand>(url_loader_factory);
#else
  return nullptr;
#endif  // BUILDFLAG(IS_WIN)
}

// static
void KeyRotationCommandFactory::SetFactoryInstanceForTesting(
    KeyRotationCommandFactory* factory) {
  DCHECK(factory);
  GetTestInstanceStorage().emplace(factory);
}

// static
void KeyRotationCommandFactory::ClearFactoryInstanceForTesting() {
  GetTestInstanceStorage().reset();
}

}  // namespace enterprise_connectors
