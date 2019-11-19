// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell_init_params.h"

#include "ash/keyboard/ui/keyboard_ui_factory.h"
#include "ash/shell_delegate.h"
#include "base/values.h"

namespace ash {

ShellInitParams::ShellInitParams() = default;

ShellInitParams::ShellInitParams(ShellInitParams&& other) = default;

ShellInitParams::~ShellInitParams() = default;

}  // namespace ash
