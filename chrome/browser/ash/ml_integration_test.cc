// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chromeos/crosier/ash_integration_test.h"

#include "chrome/test/base/chromeos/crosier/upstart.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#include "chromeos/services/machine_learning/public/mojom/model.mojom.h"

namespace chromeos::machine_learning {

namespace {

using MLIntegrationTest = AshIntegrationTest;

}  // namespace

IN_PROC_BROWSER_TEST_F(MLIntegrationTest, Bootstrap) {
  std::string ml_service = "ml-service";
  std::vector<std::string> extra_args{"TASK=mojo_service"};

  // Ensure the ml-service is stopped before testing startup.
  ASSERT_TRUE(upstart::StopJob(ml_service, extra_args));
  ASSERT_TRUE(upstart::WaitForJobStatus(
      ml_service, upstart::Goal::kStop, upstart::State::kWaiting,
      upstart::WrongGoalPolicy::kReject, extra_args));

  base::RunLoop run_loop;
  mojo::Remote<chromeos::machine_learning::mojom::Model> model;

  // Load a model. This should start the ML service.
  chromeos::machine_learning::ServiceConnection::GetInstance()
      ->GetMachineLearningService()
      .LoadBuiltinModel(
          chromeos::machine_learning::mojom::BuiltinModelSpec::New(
              chromeos::machine_learning::mojom::BuiltinModelId::TEST_MODEL),
          model.BindNewPipeAndPassReceiver(),
          base::BindLambdaForTesting(
              [&run_loop](mojom::LoadModelResult result) {
                EXPECT_EQ(result, mojom::LoadModelResult::OK);
                run_loop.Quit();
              }));

  // Catch any errors talking to the ML service.
  model.set_disconnect_handler(
      base::BindOnce([]() { FAIL() << "ML service connection error"; }));

  // LoadBuiltinModel() above will quit the loop when the model is loaded.
  run_loop.Run();

  // Verify that the service was started.
  ASSERT_TRUE(upstart::WaitForJobStatus(
      ml_service, upstart::Goal::kStart, upstart::State::kRunning,
      upstart::WrongGoalPolicy::kReject, extra_args));
}

}  // namespace chromeos::machine_learning
