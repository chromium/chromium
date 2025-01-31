// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/fre_util.h"

#include <algorithm>
#include <cstddef>

#include "base/command_line.h"
#include "base/strings/string_split.h"
#include "chrome/browser/glic/launcher/glic_launcher_configuration.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "ui/base/accelerators/command.h"
#include "url/gurl.h"

namespace glic {

GURL GetFreURL() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  bool hasGlicFreURL = command_line->HasSwitch(::switches::kGlicFreURL);
  return GURL(hasGlicFreURL
                  ? command_line->GetSwitchValueASCII(::switches::kGlicFreURL)
                  : features::kGlicFreURL.Get());
}

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
  std::string formatted_hotkey_string =
      hotkey_tokens.empty() ? "" : hotkey_tokens.front();
  for (size_t i = 1; i < hotkey_tokens.size(); i++) {
    formatted_hotkey_string += "-";
    formatted_hotkey_string += hotkey_tokens[i];
  }

  return formatted_hotkey_string;
}

}  // namespace glic
