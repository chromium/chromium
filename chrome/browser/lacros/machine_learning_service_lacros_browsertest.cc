// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/services/machine_learning/public/cpp/fake_service_connection.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom-shared.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom-test-utils.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/model.mojom-forward.h"
#include "chromeos/services/machine_learning/public/mojom/model.mojom-shared.h"
#include "content/public/test/browser_test.h"

namespace {
using chromeos::machine_learning::mojom::BuiltinModelId;
using chromeos::machine_learning::mojom::BuiltinModelSpec;
using chromeos::machine_learning::mojom::LoadModelResult;
using chromeos::machine_learning::mojom::MachineLearningService;
using chromeos::machine_learning::mojom::MachineLearningServiceAsyncWaiter;
using chromeos::machine_learning::mojom::Model;

using MachineLearningServiceLacrosBrowserTest = InProcessBrowserTest;
}  // namespace

// Tests chromeos::LacrosService::GetRemote<c::m::mojom::MachineLearningService>
// works. This should not be used by the customers directly, instead they should
// use
//   chromeos::machine_learning::ServiceConnection::GetInstance()
//       ->GetMachineLearningService();
// But we should keep this test because it initializes LacrosService, which is
// necessary before calling ServiceConnection::GetInstance().
IN_PROC_BROWSER_TEST_F(MachineLearningServiceLacrosBrowserTest,
                       CallViaLacrosService) {
  uint32_t interface_version =
      chromeos::LacrosService::Get()
          ->GetInterfaceVersion<MachineLearningService>();

  // LoadBuiltinModel doesn't have MinVersion tag in mojom, so it gets 0 as min
  // version implicitly.
  EXPECT_EQ(MachineLearningService::kLoadBuiltinModelMinVersion, 0u);
  ASSERT_GE(interface_version,
            MachineLearningService::kLoadBuiltinModelMinVersion);

  MachineLearningServiceAsyncWaiter async_waiter(
      chromeos::LacrosService::Get()
          ->GetRemote<
              chromeos::machine_learning::mojom::MachineLearningService>()
          .get());

  mojo::Remote<Model> model_remote;
  LoadModelResult result;
  async_waiter.LoadBuiltinModel(
      BuiltinModelSpec::New(BuiltinModelId::TEST_MODEL),
      model_remote.BindNewPipeAndPassReceiver(), &result);
  EXPECT_EQ(result, LoadModelResult::OK);
}

// Tests the pre-established MachineLearningService works.
IN_PROC_BROWSER_TEST_F(MachineLearningServiceLacrosBrowserTest,
                       GetMachineLearningService) {
  auto* service_connection =
      chromeos::machine_learning::ServiceConnection::GetInstance();
  MachineLearningServiceAsyncWaiter async_waiter(
      &service_connection->GetMachineLearningService());

  mojo::Remote<Model> model_remote;
  LoadModelResult result;
  async_waiter.LoadBuiltinModel(
      BuiltinModelSpec::New(BuiltinModelId::TEST_MODEL),
      model_remote.BindNewPipeAndPassReceiver(), &result);
  EXPECT_EQ(result, LoadModelResult::OK);
}

// Tests a new remote bound with BindMachineLearningService works.
IN_PROC_BROWSER_TEST_F(MachineLearningServiceLacrosBrowserTest,
                       BindMachineLearningService) {
  mojo::Remote<MachineLearningService> ml_service;
  ASSERT_FALSE(ml_service.is_bound());

  auto* service_connection =
      chromeos::machine_learning::ServiceConnection::GetInstance();
  service_connection->BindMachineLearningService(
      ml_service.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(ml_service.is_bound());

  MachineLearningServiceAsyncWaiter async_waiter(ml_service.get());

  mojo::Remote<Model> model_remote;
  LoadModelResult result;
  async_waiter.LoadBuiltinModel(
      BuiltinModelSpec::New(BuiltinModelId::TEST_MODEL),
      model_remote.BindNewPipeAndPassReceiver(), &result);
  EXPECT_EQ(result, LoadModelResult::OK);
}

// Tests FakeServiceConnectionImpl works. This doesn't require crosapi.
IN_PROC_BROWSER_TEST_F(MachineLearningServiceLacrosBrowserTest,
                       FakeServiceConnecctionImpl) {
  chromeos::machine_learning::FakeServiceConnectionImpl fake_service_connection;
  chromeos::machine_learning::ServiceConnection::
      UseFakeServiceConnectionForTesting(&fake_service_connection);

  auto* service_connection =
      chromeos::machine_learning::ServiceConnection::GetInstance();
  service_connection->Initialize();

  mojo::Remote<MachineLearningService> ml_service;
  ASSERT_FALSE(ml_service.is_bound());

  service_connection->BindMachineLearningService(
      ml_service.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(ml_service.is_bound());

  MachineLearningServiceAsyncWaiter async_waiter(ml_service.get());

  // By default, the FakeServiceConnectionImpl responds to all method calls with
  // success.
  mojo::Remote<Model> model_remote;
  LoadModelResult result;
  async_waiter.LoadBuiltinModel(
      BuiltinModelSpec::New(BuiltinModelId::TEST_MODEL),
      model_remote.BindNewPipeAndPassReceiver(), &result);
  EXPECT_EQ(result, LoadModelResult::OK);

  // It can also simulate failure.
  model_remote.reset();
  fake_service_connection.SetLoadModelFailure();
  async_waiter.LoadBuiltinModel(
      BuiltinModelSpec::New(BuiltinModelId::TEST_MODEL),
      model_remote.BindNewPipeAndPassReceiver(), &result);
  EXPECT_EQ(result, LoadModelResult::LOAD_MODEL_ERROR);
}
