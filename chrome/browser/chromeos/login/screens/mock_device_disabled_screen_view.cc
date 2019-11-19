// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/mock_device_disabled_screen_view.h"

#include "chrome/browser/chromeos/login/screens/device_disabled_screen.h"

using ::testing::AtLeast;
using ::testing::AtMost;
using ::testing::NotNull;

namespace chromeos {

MockDeviceDisabledScreenView::MockDeviceDisabledScreenView()
    : delegate_(nullptr) {
  EXPECT_CALL(*this, MockSetDelegate(NotNull())).Times(AtLeast(1));
  EXPECT_CALL(*this, MockSetDelegate(nullptr)).Times(AtMost(1));
}

MockDeviceDisabledScreenView::~MockDeviceDisabledScreenView() {
  if (delegate_)
    delegate_->OnViewDestroyed(this);
}

void MockDeviceDisabledScreenView::SetDelegate(DeviceDisabledScreen* delegate) {
  delegate_ = delegate;
  MockSetDelegate(delegate);
}

}  // namespace chromeos
