// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/glanceables_welcome_label.h"

#include <memory>

#include "ash/test/ash_test_base.h"
#include "components/session_manager/session_manager_types.h"
#include "components/user_manager/user_type.h"

namespace ash {
namespace {

// TODO(crbug.com/crbug.com/1353119): Move this to the GlanceablesTest suite
// after that suite switches to AshTestBase. These tests only pass because this
// suite is not enabling the Glanceables feature flag. When the flag is enabled
// the simulated login causes a weather fetch, which crashes.
class GlanceablesWelcomeLabelTest : public NoSessionAshTestBase {
 public:
  void SetUp() override {
    NoSessionAshTestBase::SetUp();
    SimulateUserLogin();
    label_ = std::make_unique<GlanceablesWelcomeLabel>();
  }

  void SimulateUserLogin() {
    auto* session = GetSessionControllerClient();
    session->AddUserSession("johndoe@gmail.com",
                            user_manager::UserType::USER_TYPE_REGULAR, true,
                            false, "John");
    session->SwitchActiveUser(AccountId::FromUserEmail("johndoe@gmail.com"));
    session->SetSessionState(session_manager::SessionState::ACTIVE);
  }

 protected:
  std::unique_ptr<GlanceablesWelcomeLabel> label_;
};

TEST_F(GlanceablesWelcomeLabelTest, RendersCorrectText) {
  EXPECT_EQ(label_->GetText(), u"Welcome back, John");
}

}  // namespace
}  // namespace ash
