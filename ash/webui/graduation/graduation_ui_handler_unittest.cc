// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/graduation/graduation_ui_handler.h"

#include <memory>

#include "ash/webui/graduation/mojom/graduation_ui.mojom.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/values.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::graduation {

namespace {
constexpr char kUserGaiaId[] = "111";
constexpr char kUserEmail[] = "user1test@gmail.com";
}  // namespace

class GraduationUiHandlerTest : public testing::Test {
 public:
  GraduationUiHandlerTest()
      : handler_(std::make_unique<GraduationUiHandler>(
            handler_remote_.BindNewPipeAndPassReceiver())) {}

  ~GraduationUiHandlerTest() override = default;

  void SetUp() override {
    auto account_id = AccountId::FromUserEmailGaiaId(kUserEmail, kUserGaiaId);
    fake_user_manager_.Reset(std::make_unique<user_manager::FakeUserManager>());
    fake_user_manager_->AddUser(account_id);
  }

  GraduationUiHandler* handler() const { return handler_.get(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  mojo::Remote<graduation_ui::mojom::GraduationUiHandler> handler_remote_;
  const std::unique_ptr<GraduationUiHandler> handler_;
  user_manager::TypedScopedUserManager<user_manager::FakeUserManager>
      fake_user_manager_;
};

TEST_F(GraduationUiHandlerTest, GetProfileInfo) {
  base::RunLoop run_loop;
  handler()->GetProfileInfo(base::BindLambdaForTesting(
      [&](graduation_ui::mojom::ProfileInfoPtr profile_info) -> void {
        EXPECT_EQ(kUserEmail, profile_info->email);
        EXPECT_FALSE(profile_info->photo_url.empty());
        run_loop.Quit();
      }));
  run_loop.Run();
}
}  // namespace ash::graduation
