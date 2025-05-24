// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TEST_MOCK_SESSION_CONTROLLER_H_
#define ASH_PUBLIC_CPP_TEST_MOCK_SESSION_CONTROLLER_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "ash/public/cpp/session/session_controller.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockSessionController : public ash::SessionController {
 public:
  MockSessionController();
  MockSessionController(const MockSessionController&) = delete;
  MockSessionController& operator=(const MockSessionController&) = delete;
  ~MockSessionController() override;

  // SessionController:
  MOCK_METHOD(void, SetClient, (SessionControllerClient * client), (override));
  MOCK_METHOD(void, SetSessionInfo, (const SessionInfo& info), (override));
  MOCK_METHOD(void,
              UpdateUserSession,
              (const UserSession& user_session),
              (override));
  MOCK_METHOD(void,
              SetUserSessionOrder,
              (const std::vector<uint32_t>& user_session_ids),
              (override));
  MOCK_METHOD(void,
              PrepareForLock,
              (PrepareForLockCallback callback),
              (override));
  MOCK_METHOD(void, StartLock, (StartLockCallback callback), (override));
  MOCK_METHOD(void, NotifyChromeLockAnimationsComplete, (), (override));
  MOCK_METHOD(void,
              RunUnlockAnimation,
              (RunUnlockAnimationCallback callback),
              (override));
  MOCK_METHOD(void, NotifyChromeTerminating, (), (override));
  MOCK_METHOD(void,
              SetSessionLengthLimit,
              (base::TimeDelta length_limit, base::Time start_time),
              (override));
  MOCK_METHOD(void,
              CanSwitchActiveUser,
              (CanSwitchActiveUserCallback callback),
              (override));
  MOCK_METHOD(void,
              ShowMultiprofilesIntroDialog,
              (ShowMultiprofilesIntroDialogCallback callback),
              (override));
  MOCK_METHOD(void,
              ShowTeleportWarningDialog,
              (ShowTeleportWarningDialogCallback callback),
              (override));
  MOCK_METHOD(void,
              ShowMultiprofilesSessionAbortedDialog,
              (const std::string& user_email),
              (override));
  MOCK_METHOD(void,
              AddSessionActivationObserverForAccountId,
              (const AccountId& account_id,
               SessionActivationObserver* observer),
              (override));
  MOCK_METHOD(void,
              RemoveSessionActivationObserverForAccountId,
              (const AccountId& account_id,
               SessionActivationObserver* observer),
              (override));
  MOCK_METHOD(void, AddObserver, (SessionObserver * observer), (override));
  MOCK_METHOD(void, RemoveObserver, (SessionObserver * observer), (override));
  MOCK_METHOD(bool, IsScreenLocked, (), (const, override));
  MOCK_METHOD(std::optional<int>, GetExistingUsersCount, (), (const, override));
  MOCK_METHOD(void, NotifyFirstSessionReady, (), (override));
  MOCK_METHOD(void, NotifyUserToBeRemoved, (const AccountId&), (override));
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TEST_MOCK_SESSION_CONTROLLER_H_
