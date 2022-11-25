// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_INPUT_METHOD_CONFIGURATION_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_INPUT_METHOD_CONFIGURATION_H_

#include "base/task/sequenced_task_runner.h"

namespace ash {
namespace input_method {

class InputMethodManager;

void Initialize();

// Similar to Initialize(), but can inject an alternative InputMethodManager
// such as MockInputMethodManager for testing. The injected object will be
// owned by the internal pointer and deleted by Shutdown().
// TODO(nona): Remove this and use InputMethodManager::Initialize instead.
void InitializeForTesting(InputMethodManager* mock_manager);

// Disables the IME extension loading (e.g. for browser tests).
void DisableExtensionLoading();

void Shutdown();

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_INPUT_METHOD_CONFIGURATION_H_
