// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/tpm/prepare_tpm.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/tpm_manager/tpm_manager.pb.h"
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

class PrepareTpmTest : public ::testing::Test {
 public:
  PrepareTpmTest() { TpmManagerClient::InitializeFake(); }
  ~PrepareTpmTest() override { TpmManagerClient::Shutdown(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment;
};

}  // namespace

// Tests if the password is getting cleared when TPM is owned.
TEST_F(PrepareTpmTest, PrepareTpmOwned) {
  TpmManagerClient::Get()
      ->GetTestInterface()
      ->mutable_nonsensitive_status_reply()
      ->set_is_owned(true);

  base::RunLoop run_loop;
  auto on_finished = base::BindOnce(
      [](base::RunLoop* run_loop) { run_loop->Quit(); }, &run_loop);
  PrepareTpm(std::move(on_finished));
  run_loop.Run();

  EXPECT_EQ(TpmManagerClient::Get()
                ->GetTestInterface()
                ->clear_stored_owner_password_count(),
            1);
}

// Tests if the ownership process is triggered if TPM is not owned yet.
TEST_F(PrepareTpmTest, PrepareTpmNotOwned) {
  TpmManagerClient::Get()
      ->GetTestInterface()
      ->mutable_nonsensitive_status_reply()
      ->set_is_owned(false);

  base::RunLoop run_loop;
  auto on_finished = base::BindOnce(
      [](base::RunLoop* run_loop) { run_loop->Quit(); }, &run_loop);
  PrepareTpm(std::move(on_finished));
  run_loop.Run();

  EXPECT_EQ(TpmManagerClient::Get()->GetTestInterface()->take_ownership_count(),
            1);
}

// Tests the program flow doesn't fall through and execute any unexpected
// follow-up action if tpm manager reports error.
TEST_F(PrepareTpmTest, PrepareTpmFailedToGetStatus) {
  TpmManagerClient::Get()
      ->GetTestInterface()
      ->mutable_nonsensitive_status_reply()
      ->set_status(::tpm_manager::STATUS_DBUS_ERROR);

  base::RunLoop run_loop;
  auto on_finished = base::BindOnce(
      [](base::RunLoop* run_loop) { run_loop->Quit(); }, &run_loop);
  PrepareTpm(std::move(on_finished));
  run_loop.Run();

  EXPECT_EQ(TpmManagerClient::Get()
                ->GetTestInterface()
                ->clear_stored_owner_password_count(),
            0);
  EXPECT_EQ(TpmManagerClient::Get()->GetTestInterface()->take_ownership_count(),
            0);
}

}  // namespace ash
