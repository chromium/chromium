// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/in_session_auth/authentication_dialog.h"

#include "ash/test/ash_test_base.h"
#include "base/test/bind.h"
#include "ui/views/controls/textfield/textfield.h"

namespace ash {
namespace {

class AuthenticationDialogTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();
    dialog_ = AuthenticationDialog::Show(base::BindLambdaForTesting(
        [&](AuthenticationDialog::Result result, const std::u16string& token,
            base::TimeDelta timeout) {
          result_ = result;
          called_ = true;
        }));
  }

 protected:
  bool called_ = false;
  AuthenticationDialog::Result result_;
  AuthenticationDialog* dialog_;
};

TEST_F(AuthenticationDialogTest, CallbackCalledOnCancel) {
  dialog_->Cancel();
  EXPECT_TRUE(called_);
  EXPECT_EQ(result_, AuthenticationDialog::Result::kAborted);
}

TEST_F(AuthenticationDialogTest, CallbackCalledOnClose) {
  dialog_->Close();
  EXPECT_TRUE(called_);
  EXPECT_EQ(result_, AuthenticationDialog::Result::kAborted);
}

}  // namespace
}  // namespace ash