// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_LOGIN_AUTH_MOCK_AUTH_PERFORMER_H_
#define ASH_COMPONENTS_LOGIN_AUTH_MOCK_AUTH_PERFORMER_H_

#include "ash/components/login/auth/auth_performer.h"
#include "ash/components/login/auth/user_context.h"
#include "chromeos/dbus/userdataauth/userdataauth_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockAuthPerformer : public AuthPerformer {
 public:
  explicit MockAuthPerformer(base::raw_ptr<UserDataAuthClient> client);
  MockAuthPerformer(const MockAuthPerformer&) = delete;
  MockAuthPerformer& operator=(const MockAuthPerformer&) = delete;
  ~MockAuthPerformer() override;

  // AuthPerformer
  MOCK_METHOD(void,
              StartAuthSession,
              (std::unique_ptr<UserContext> context,
               bool ephemeral,
               StartSessionCallback callback),
              (override));

  MOCK_METHOD(void,
              AuthenticateWithPassword,
              (const std::string& key_label,
               const std::string& password,
               std::unique_ptr<UserContext> context,
               AuthOperationCallback callback),
              (override));
};

}  // namespace ash

#endif  // ASH_COMPONENTS_LOGIN_AUTH_AUTH_PERFORMER_H_
