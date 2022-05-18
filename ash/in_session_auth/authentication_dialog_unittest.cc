// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/in_session_auth/authentication_dialog.h"

#include "ash/test/ash_test_base.h"
#include "base/logging.h"
#include "base/test/bind.h"
#include "base/unguessable_token.h"
#include "ui/views/controls/textfield/textfield.h"

namespace ash {
namespace {

class AuthenticationDialogTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();
    dialog_ = new AuthenticationDialog(base::BindLambdaForTesting(
        [&](bool success, const base::UnguessableToken& token,
            base::TimeDelta timeout) { success_ = success; }));
    dialog_->Show();
  }

 protected:
  absl::optional<bool> success_;
  base::raw_ptr<AuthenticationDialog> dialog_;
};

TEST_F(AuthenticationDialogTest, CallbackCalledOnCancel) {
  dialog_->Cancel();
  EXPECT_TRUE(success_.has_value());
  EXPECT_EQ(success_.value(), false);
}

TEST_F(AuthenticationDialogTest, CallbackCalledOnClose) {
  dialog_->Close();
  EXPECT_TRUE(success_.has_value());
  EXPECT_EQ(success_.value(), false);
}

}  // namespace
}  // namespace ash
