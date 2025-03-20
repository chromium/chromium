// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_HOTKEY_H_
#define CHROME_BROWSER_GLIC_GLIC_HOTKEY_H_

#include <string>

namespace glic {

// Util function shared by both the FRE and the Host for communicating the OS
// Hotkey to the web implementations.
std::string GetHotkeyString();

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_HOTKEY_H_
