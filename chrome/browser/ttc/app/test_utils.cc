// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/app/test_utils.h"

namespace ttc {

MockTtcBackend::MockTtcBackend() {
  ON_CALL(*this, Connect).WillByDefault([this](TtcBackend::Observer* observer) {
    observer_ = observer;
    is_connected_ = true;
  });
  ON_CALL(*this, Close()).WillByDefault([this]() { is_connected_ = false; });
  ON_CALL(*this, is_connected()).WillByDefault([this]() {
    return is_connected_;
  });
}

MockTtcBackend::~MockTtcBackend() = default;

}  // namespace ttc
