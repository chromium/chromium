// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_IN_SESSION_AUTH_MOCK_IN_SESSION_AUTH_DIALOG_CLIENT_H_
#define ASH_IN_SESSION_AUTH_MOCK_IN_SESSION_AUTH_DIALOG_CLIENT_H_

#include <string>

#include "ash/public/cpp/in_session_auth_dialog_client.h"
#include "ash/public/cpp/login_types.h"
#include "base/functional/callback.h"
#include "components/account_id/account_id.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockInSessionAuthDialogClient : public InSessionAuthDialogClient {
 public:
  MockInSessionAuthDialogClient();
  MockInSessionAuthDialogClient(const MockInSessionAuthDialogClient&) = delete;
  MockInSessionAuthDialogClient& operator=(
      const MockInSessionAuthDialogClient&) = delete;
  ~MockInSessionAuthDialogClient() override;

  // InSessionAuthDialogClient:
  MOCK_METHOD(void,
              StartAuthSession,
              (base::OnceCallback<void(bool)>),
              (override));

  MOCK_METHOD(void, InvalidateAuthSession, (), (override));

  MOCK_METHOD(void,
              AuthenticateUserWithPasswordOrPin,
              (const std::string& password,
               bool authenticated_by_pin,
               base::OnceCallback<void(bool)> callback),
              (override));

  MOCK_METHOD(bool,
              IsFingerprintAuthAvailable,
              (const AccountId& account_id),
              (override));

  MOCK_METHOD(void,
              StartFingerprintAuthSession,
              (const AccountId& account_id,
               base::OnceCallback<void(bool)> callback),
              (override));

  MOCK_METHOD(void,
              EndFingerprintAuthSession,
              (base::OnceClosure callback),
              (override));

  MOCK_METHOD(void,
              CheckPinAuthAvailability,
              (const AccountId& account_id,
               base::OnceCallback<void(bool)> callback),
              (override));

  MOCK_METHOD(void,
              AuthenticateUserWithFingerprint,
              (base::OnceCallback<void(bool, FingerprintState)> callback),
              (override));

  MOCK_METHOD(aura::Window*, OpenInSessionAuthHelpPage, (), (const override));
};

}  // namespace ash

#endif  // ASH_IN_SESSION_AUTH_MOCK_IN_SESSION_AUTH_DIALOG_CLIENT_H_
