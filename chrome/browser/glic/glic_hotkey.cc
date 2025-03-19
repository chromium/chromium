// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_hotkey.h"

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/browser/background/glic/glic_launcher_configuration.h"
#include "ui/base/accelerators/command.h"

namespace glic {
std::string GetHotkeyString() {
  // If the hotkey is unset, return an empty string as its representation.
  std::string hotkey_string = ui::Command::AcceleratorToString(
      glic::GlicLauncherConfiguration::GetGlobalHotkey());
  if (hotkey_string.empty()) {
    return "";
  }

  // Format the accelerator string so that it can be passed to the glic WebUI
  // as a URL query parameter. Specifically, each component of the accelerator
  // will be demarked with the '<' and '>' characters, and all components will
  // then be joined with the '-' character.
  std::vector<std::string> hotkey_tokens = base::SplitString(
      hotkey_string, "+", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (std::string& token : hotkey_tokens) {
    token = "<" + token + ">";
  }

  // Build the formatted string starting with the first token. There should
  // always be at least two tokens in the accelerator.
  return base::JoinString(hotkey_tokens, "-");
}

}  // namespace glic
