// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/test/device_trust_test_environment_win.h"

#include <windows.h>

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/command_line.h"
#include "base/notreached.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/win_key_rotation_command.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/mock_key_network_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/shared_command_constants.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/key_rotation_manager.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/management_service/rotate_util.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/util_constants.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::Invoke;
using testing::StrictMock;

using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;

namespace enterprise_connectors {

using test::MockKeyNetworkDelegate;
using HttpResponseCode = KeyNetworkDelegate::HttpResponseCode;

constexpr HttpResponseCode kSuccessCode = 200;

HRESULT MockRunGoogleUpdateElevatedCommandFn(
    HttpResponseCode upload_response_code,
    std::string expected_dm_token,
    std::string expected_client_id,
    const wchar_t* command,
    const std::vector<std::string>& args,
    std::optional<DWORD>* return_code) {
  base::CommandLine cmd_line(base::CommandLine::NO_PROGRAM);
  CHECK(args.size() == 3);
  cmd_line.AppendSwitchASCII(switches::kRotateDTKey, args[0]);
  cmd_line.AppendSwitchASCII(switches::kDmServerUrl, args[1]);
  cmd_line.AppendSwitchASCII(switches::kNonce, args[2]);
  auto mock_network_delegate =
      std::make_unique<StrictMock<MockKeyNetworkDelegate>>();
  EXPECT_CALL(*mock_network_delegate, SendPublicKeyToDmServer(_, _, _, _))
      .WillOnce(Invoke(
          [upload_response_code, expected_dm_token, expected_client_id, args](
              const GURL& url, const std::string& dm_token,
              const std::string& body, base::OnceCallback<void(int)> callback) {
            // Check if the DM Server URL contains the correct Client ID
            CHECK(url.spec().find(expected_client_id) != std::string::npos);
            // Check if the correct DM Token is being uploaded
            CHECK_EQ(dm_token, expected_dm_token);
            // TODO(b/269746642): add a check for the 'body' parameter above

            std::move(callback).Run(upload_response_code);
          }));
  const auto result = enterprise_connectors::RotateDeviceTrustKey(
      enterprise_connectors::KeyRotationManager::Create(
          std::move(mock_network_delegate)),
      cmd_line, install_static::GetChromeChannel());
  switch (result) {
    case enterprise_connectors::KeyRotationResult::kSucceeded:
      *return_code = installer::ROTATE_DTKEY_SUCCESS;
      break;
    case enterprise_connectors::KeyRotationResult::kInsufficientPermissions:
      *return_code = installer::ROTATE_DTKEY_FAILED_PERMISSIONS;
      break;
    case enterprise_connectors::KeyRotationResult::kFailedKeyConflict:
      *return_code = installer::ROTATE_DTKEY_FAILED_CONFLICT;
      break;
    case enterprise_connectors::KeyRotationResult::kFailed:
    default:
      *return_code = installer::ROTATE_DTKEY_FAILED;
      break;
  }

  return S_OK;
}

DeviceTrustTestEnvironmentWin::DeviceTrustTestEnvironmentWin()
    : DeviceTrustTestEnvironment("device_trust_test_environment_win",
                                 kSuccessCode),
      install_details_(true) {
  KeyRotationCommandFactory::SetFactoryInstanceForTesting(this);
}

DeviceTrustTestEnvironmentWin::~DeviceTrustTestEnvironmentWin() {
  KeyRotationCommandFactory::ClearFactoryInstanceForTesting();
}

std::unique_ptr<KeyRotationCommand>
DeviceTrustTestEnvironmentWin::CreateCommand(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    policy::DeviceManagementService* device_management_service) {
  if (!worker_thread_.IsRunning()) {
    // Make sure the worker thread is running. Its task runner can be reused for
    // all created commands, and its destruction will be handled automatically.
    bool started = worker_thread_.StartAndWaitForTesting();
    CHECK(started);
  }
  return std::make_unique<WinKeyRotationCommand>(
      base::BindRepeating(&MockRunGoogleUpdateElevatedCommandFn,
                          upload_response_code_, expected_dm_token_,
                          expected_client_id_),
      worker_thread_.task_runner());
}

void DeviceTrustTestEnvironmentWin::SetUpExistingKey() {
  auto trust_level = BPKUR::CHROME_BROWSER_HW_KEY;
  auto key_pair = key_persistence_delegate_->CreateKeyPair();
  EXPECT_TRUE(key_persistence_delegate_->StoreKeyPair(
      trust_level, key_pair->key()->GetWrappedKey()));
}

void DeviceTrustTestEnvironmentWin::ClearExistingKey() {
  EXPECT_TRUE(key_persistence_delegate_->StoreKeyPair(
      BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED, std::vector<uint8_t>()));

  EXPECT_FALSE(KeyExists());
}

std::vector<uint8_t> DeviceTrustTestEnvironmentWin::GetWrappedKey() {
  std::vector<uint8_t> wrapped_key;
  auto loaded_key_pair = key_persistence_delegate_->LoadKeyPair(
      KeyStorageType::kPermanent, nullptr);
  if (loaded_key_pair) {
    auto* key_pointer = loaded_key_pair->key();
    if (key_pointer) {
      wrapped_key = key_pointer->GetWrappedKey();
    }
  }

  return wrapped_key;
}

}  // namespace enterprise_connectors
