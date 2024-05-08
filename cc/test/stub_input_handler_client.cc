// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/stub_input_handler_client.h"

namespace cc {

bool StubInputHandlerClient::HasQueuedInput() const {
  return false;
}

}  // namespace cc
