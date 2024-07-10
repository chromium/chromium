// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AUTH_VIEWS_TEST_SUPPORT_MOCK_ACTIVE_SESSION_AUTH_VIEW_OBSERVER_H_
#define ASH_AUTH_VIEWS_TEST_SUPPORT_MOCK_ACTIVE_SESSION_AUTH_VIEW_OBSERVER_H_

#include <cstdint>
#include <string>

#include "ash/ash_export.h"
#include "ash/auth/views/active_session_auth_view.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class ASH_EXPORT MockActiveSessionAuthViewObserver
    : public ActiveSessionAuthView::Observer {
 public:
  MockActiveSessionAuthViewObserver();
  ~MockActiveSessionAuthViewObserver() override;

  MOCK_METHOD(void, OnPasswordSubmit, (const std::u16string&), (override));
  MOCK_METHOD(void, OnPinSubmit, (const std::u16string&), (override));
  MOCK_METHOD(void, OnClose, (), (override));
};

}  // namespace ash

#endif  // ASH_AUTH_VIEWS_TEST_SUPPORT_MOCK_ACTIVE_SESSION_AUTH_VIEW_OBSERVER_H_
