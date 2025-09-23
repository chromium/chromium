// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_MOCK_WINDOW_CONTROLLER_LIST_OBSERVER_H_
#define CHROME_BROWSER_EXTENSIONS_MOCK_WINDOW_CONTROLLER_LIST_OBSERVER_H_

#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/extensions/window_controller_list_observer.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {
class MockWindowControllerListObserver final
    : public WindowControllerListObserver {
 public:
  MockWindowControllerListObserver();
  ~MockWindowControllerListObserver() override;

  MOCK_METHOD(void, OnWindowControllerAdded, (WindowController*), (override));
  MOCK_METHOD(void, OnWindowControllerRemoved, (WindowController*), (override));
  MOCK_METHOD(void, OnWindowBoundsChanged, (WindowController*), (override));
  MOCK_METHOD(void,
              OnWindowFocusChanged,
              (WindowController*, bool),
              (override));
};
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_MOCK_WINDOW_CONTROLLER_LIST_OBSERVER_H_
