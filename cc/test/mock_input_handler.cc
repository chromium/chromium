// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/mock_input_handler.h"

#include "base/no_destructor.h"

namespace cc {

namespace {

FakeCompositorDelegateForInput& GetFakeCompositorDelegate() {
  static base::NoDestructor<FakeCompositorDelegateForInput>
      fake_compositor_delegate;
  return *fake_compositor_delegate;
}

}  // namespace

MockInputHandler::MockInputHandler()
    : InputHandler(GetFakeCompositorDelegate()) {}

MockInputHandler::~MockInputHandler() = default;

}  // namespace cc
