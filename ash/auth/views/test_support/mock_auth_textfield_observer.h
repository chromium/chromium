// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AUTH_VIEWS_TEST_SUPPORT_MOCK_AUTH_TEXTFIELD_OBSERVER_H_
#define ASH_AUTH_VIEWS_TEST_SUPPORT_MOCK_AUTH_TEXTFIELD_OBSERVER_H_

#include <cstdint>
#include <string>

#include "ash/ash_export.h"
#include "ash/auth/views/auth_textfield.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class ASH_EXPORT MockAuthTextfieldObserver : public AuthTextfield::Observer {
 public:
  MockAuthTextfieldObserver();
  ~MockAuthTextfieldObserver() override;

  MOCK_METHOD(void, OnTextfieldBlur, (), (override));
  MOCK_METHOD(void, OnTextfieldFocus, (), (override));
  MOCK_METHOD(void, OnContentsChanged, (const std::u16string&), (override));
  MOCK_METHOD(void, OnTextVisibleChanged, (bool), (override));
  MOCK_METHOD(void, OnSubmit, (), (override));
  MOCK_METHOD(void, OnEscape, (), (override));
};

}  // namespace ash

#endif  // ASH_AUTH_VIEWS_TEST_SUPPORT_MOCK_AUTH_TEXTFIELD_OBSERVER_H_
