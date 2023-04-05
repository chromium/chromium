// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/user_education_ash_test_base.h"

#include <memory>

#include "ash/test_shell_delegate.h"
#include "ash/user_education/mock_user_education_delegate.h"
#include "base/test/bind.h"

namespace ash {

void UserEducationAshTestBase::SetUp() {
  // Mock the `user_education_delegate_`. Note that it is expected that the user
  // education delegate be created once and only once.
  auto shell_delegate = std::make_unique<TestShellDelegate>();
  shell_delegate->SetUserEducationDelegateFactory(base::BindLambdaForTesting(
      [&]() -> std::unique_ptr<UserEducationDelegate> {
        EXPECT_EQ(user_education_delegate_, nullptr);
        auto user_education_delegate =
            std::make_unique<testing::NiceMock<MockUserEducationDelegate>>();
        user_education_delegate_ = user_education_delegate.get();
        return user_education_delegate;
      }));
  NoSessionAshTestBase::SetUp(std::move(shell_delegate));
}

}  // namespace ash
