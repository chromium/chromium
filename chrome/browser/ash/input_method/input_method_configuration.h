// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_INPUT_METHOD_CONFIGURATION_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_INPUT_METHOD_CONFIGURATION_H_

#include "base/task/sequenced_task_runner.h"
// TODO(https://crbug.com/1164001): remove and use forward declaration.
#include "ui/base/ime/ash/input_method_manager.h"

namespace ash {
namespace input_method {

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

// TODO(https://crbug.com/1164001): remove when ChromeOS code migration is done.
namespace chromeos {
namespace input_method {
using ::ash::input_method::Initialize;
using ::ash::input_method::InitializeForTesting;
using ::ash::input_method::Shutdown;
}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_INPUT_METHOD_CONFIGURATION_H_
