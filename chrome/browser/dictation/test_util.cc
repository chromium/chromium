// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/test_util.h"

#include "base/functional/bind.h"

namespace dictation {

using ::testing::_;

MockStreamProvider::MockStreamProvider() = default;
MockStreamProvider::~MockStreamProvider() = default;

MockSessionUi::MockSessionUi() = default;
MockSessionUi::~MockSessionUi() = default;

MockSessionControllerDelegate::MockSessionControllerDelegate() {
  ON_CALL(*this, CreateUi(_)).WillByDefault([]() {
    return std::make_unique<testing::NiceMock<MockSessionUi>>();
  });
  ON_CALL(*this, CreateStreamProvider(_)).WillByDefault([]() {
    return std::make_unique<testing::NiceMock<MockStreamProvider>>();
  });
}
MockSessionControllerDelegate::~MockSessionControllerDelegate() = default;

MockTarget::MockTarget() = default;
MockTarget::~MockTarget() = default;

}  // namespace dictation
