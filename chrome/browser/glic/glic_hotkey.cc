// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_hotkey.h"

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/background/glic/glic_launcher_configuration.h"
#include "ui/base/accelerators/command.h"

namespace glic {
std::string GetHotkeyString() {
  std::vector<std::u16string> hotkey_tokens =
      glic::GlicLauncherConfiguration::GetGlobalHotkey()
          .GetShortcutVectorRepresentation();
  // If the hotkey is unset, return an empty string as its representation.
  if (hotkey_tokens.empty()) {
    return "";
  }

  // Format the accelerator string so that it can be passed to the glic WebUI
  // as a URL query parameter. Specifically, each component of the accelerator
  // will be demarked with the '<' and '>' characters, and all components will
  // then be joined with the '-' character.
  for (std::u16string& token : hotkey_tokens) {
    token = u"<" + token + u">";
  }

  // Build the formatted string starting with the first token. There should
  // always be at least two tokens in the accelerator.
  return base::UTF16ToUTF8(base::JoinString(hotkey_tokens, u"-"));
}

}  // namespace glic
