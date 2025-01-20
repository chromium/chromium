// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/mock_input_handler.h"

#include "base/lazy_instance.h"

namespace cc {

namespace {

base::LazyInstance<FakeCompositorDelegateForInput>::Leaky
    g_fake_compositor_delegate = LAZY_INSTANCE_INITIALIZER;

}  // namespace

MockInputHandler::MockInputHandler()
    : InputHandler(g_fake_compositor_delegate.Get()) {}

MockInputHandler::~MockInputHandler() = default;

}  // namespace cc
