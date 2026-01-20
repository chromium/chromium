// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/test_support/mock_event_dispatcher.h"

namespace actor::ui {

MockUiEventDispatcher::MockUiEventDispatcher() = default;
MockUiEventDispatcher::~MockUiEventDispatcher() = default;

std::unique_ptr<UiEventDispatcher> NewMockUiEventDispatcher() {
  return std::make_unique<testing::NiceMock<MockUiEventDispatcher>>();
}

}  // namespace actor::ui
