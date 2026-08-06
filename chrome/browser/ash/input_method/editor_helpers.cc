// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_helpers.h"

#include <string>

#include "chrome/browser/browser_process.h"

namespace ash::input_method {

std::string GetSystemLocale() {
  return g_browser_process != nullptr
             ? g_browser_process->GetApplicationLocale()
             : "";
}

}  // namespace ash::input_method
