// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/mock_user_education_delegate.h"

namespace ash {

MockUserEducationDelegate::MockUserEducationDelegate() {
  // Provide a default implementation of `IsNewUser()` which returns that it is
  // unknown whether a given user is "new" or "existing" on invocation.
  // NOTE: this prevents the Welcome Tour from running in tests which do not:
  // (a) explicitly override this implementation to return `true`, or
  // (b) explicitly force Welcome Tour user eligibility.
  ON_CALL(*this, IsNewUser)
      .WillByDefault(::testing::ReturnRefOfCopy(std::optional<bool>()));
}

MockUserEducationDelegate::~MockUserEducationDelegate() = default;

}  // namespace ash
