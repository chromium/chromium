// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AUTH_VIEWS_TEST_SUPPORT_MOCK_AUTH_CONTAINER_VIEW_OBSERVER_H_
#define ASH_AUTH_VIEWS_TEST_SUPPORT_MOCK_AUTH_CONTAINER_VIEW_OBSERVER_H_

#include <cstdint>
#include <string>

#include "ash/ash_export.h"
#include "ash/auth/views/auth_container_view.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class ASH_EXPORT MockAuthContainerViewObserver
    : public AuthContainerView::Observer {
 public:
  MockAuthContainerViewObserver();
  ~MockAuthContainerViewObserver() override;

  MOCK_METHOD(void, OnPasswordSubmit, (const std::u16string&), (override));
  MOCK_METHOD(void, OnPinSubmit, (const std::u16string&), (override));
  MOCK_METHOD(void, OnEscape, (), (override));
  MOCK_METHOD(void, OnContentsChanged, (), (override));
};

}  // namespace ash

#endif  // ASH_AUTH_VIEWS_TEST_SUPPORT_MOCK_AUTH_CONTAINER_VIEW_OBSERVER_H_
