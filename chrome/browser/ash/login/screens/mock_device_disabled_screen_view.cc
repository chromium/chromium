// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/mock_device_disabled_screen_view.h"

#include "chrome/browser/ash/login/screens/device_disabled_screen.h"

using ::testing::AtLeast;
using ::testing::AtMost;
using ::testing::NotNull;

namespace chromeos {

MockDeviceDisabledScreenView::MockDeviceDisabledScreenView()
    : screen_(nullptr) {
  EXPECT_CALL(*this, MockBind(NotNull())).Times(AtLeast(1));
  EXPECT_CALL(*this, MockBind(nullptr)).Times(AtMost(1));
}

MockDeviceDisabledScreenView::~MockDeviceDisabledScreenView() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void MockDeviceDisabledScreenView::Bind(DeviceDisabledScreen* screen) {
  screen_ = screen;
  MockBind(screen);
}

}  // namespace chromeos
