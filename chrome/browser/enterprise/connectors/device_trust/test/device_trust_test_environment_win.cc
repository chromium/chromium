// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/test/device_trust_test_environment_win.h"

#include <windows.h>

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/command_line.h"
#include "base/notreached.h"
#include "base/task/thread_pool.h"
#include "base/win/registry.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/win_key_rotation_command.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/mock_key_network_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate_factory.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/shared_command_constants.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/key_rotation_manager.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/management_service/rotate_util.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/util_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
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
    const wchar_t* command,
    const std::vector<std::string>& args,
    DWORD* return_code) {
  base::CommandLine cmd_line(base::CommandLine::NO_PROGRAM);
  CHECK(args.size() == 3);
  cmd_line.AppendSwitchASCII(switches::kRotateDTKey, args[0]);
  cmd_line.AppendSwitchASCII(switches::kDmServerUrl, args[1]);
  cmd_line.AppendSwitchASCII(switches::kNonce, args[2]);
  auto mock_network_delegate =
      std::make_unique<StrictMock<MockKeyNetworkDelegate>>();
  EXPECT_CALL(*mock_network_delegate, SendPublicKeyToDmServer(_, _, _, _))
      .WillOnce(Invoke(
          [upload_response_code](const GURL& url, const std::string& dm_token,
                                 const std::string& body,
                                 base::OnceCallback<void(int)> callback) {
            std::move(callback).Run(upload_response_code);
          }));
  *return_code = enterprise_connectors::RotateDeviceTrustKey(
                     enterprise_connectors::KeyRotationManager::Create(
                         std::move(mock_network_delegate)),
                     cmd_line, install_static::GetChromeChannel())
                     ? installer::InstallStatus::ROTATE_DTKEY_SUCCESS
                     : installer::InstallStatus::ROTATE_DTKEY_FAILED;
  return S_OK;
}

DeviceTrustTestEnvironmentWin::DeviceTrustTestEnvironmentWin()
    : DeviceTrustTestEnvironment("device_trust_test_environment_win",
                                 kSuccessCode) {
  registry_override_manager_.OverrideRegistry(HKEY_LOCAL_MACHINE);
  KeyRotationCommandFactory::SetFactoryInstanceForTesting(this);
}

DeviceTrustTestEnvironmentWin::~DeviceTrustTestEnvironmentWin() {
  KeyRotationCommandFactory::ClearFactoryInstanceForTesting();
}

std::unique_ptr<KeyRotationCommand>
DeviceTrustTestEnvironmentWin::CreateCommand(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    PrefService* local_prefs) {
  if (!worker_thread_.IsRunning()) {
    // Make sure the worker thread is running. Its task runner can be reused for
    // all created commands, and its destruction will be handled automatically.
    bool started = worker_thread_.StartAndWaitForTesting();
    CHECK(started);
  }
  return std::make_unique<WinKeyRotationCommand>(
      base::BindRepeating(&MockRunGoogleUpdateElevatedCommandFn,
                          upload_response_code_),
      worker_thread_.task_runner());
}

void DeviceTrustTestEnvironmentWin::SetUploadResult(
    HttpResponseCode upload_response_code) {
  upload_response_code_ = upload_response_code;
}

void DeviceTrustTestEnvironmentWin::SetUpExistingKey() {
  auto* factory = KeyPersistenceDelegateFactory::GetInstance();
  auto trust_level = BPKUR::CHROME_BROWSER_HW_KEY;
  std::unique_ptr<KeyPersistenceDelegate> win_key_persistence_delegate =
      factory->CreateKeyPersistenceDelegate();
  auto key_pair = win_key_persistence_delegate->CreateKeyPair();
  EXPECT_TRUE(win_key_persistence_delegate->StoreKeyPair(
      trust_level, key_pair->key()->GetWrappedKey()));
}

}  // namespace enterprise_connectors
