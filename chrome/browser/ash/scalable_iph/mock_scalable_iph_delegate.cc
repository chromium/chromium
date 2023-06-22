// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scalable_iph/mock_scalable_iph_delegate.h"

namespace ash {
namespace test {

MockScalableIphDelegate::MockScalableIphDelegate() = default;
MockScalableIphDelegate::~MockScalableIphDelegate() = default;

void MockScalableIphDelegate::DelegateObserverWith(
    std::unique_ptr<scalable_iph::ScalableIphDelegate> delegate) {
  CHECK(!delegate_) << "You are NOT allowed to set a delegate object twice";

  delegate_ = std::move(delegate);

  ON_CALL(*this, AddObserver).WillByDefault([this](Observer* observer) {
    delegate_->AddObserver(observer);
  });

  ON_CALL(*this, RemoveObserver).WillByDefault([this](Observer* observer) {
    delegate_->RemoveObserver(observer);
  });

  ON_CALL(*this, IsOnline).WillByDefault([this]() {
    return delegate_->IsOnline();
  });
}

}  // namespace test
}  // namespace ash
