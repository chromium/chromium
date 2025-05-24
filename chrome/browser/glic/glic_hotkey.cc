// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_hotkey.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/background/glic/glic_launcher_configuration.h"
#include "ui/base/accelerators/command.h"

namespace glic {

namespace {

std::string GetHotkeyStringWithMapping(
    base::RepeatingCallback<void(std::u16string&)> token_mapping) {
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
    token_mapping.Run(token);
    token = u"<" + token + u">";
  }

  // Build the formatted string starting with the first token. There should
  // always be at least two tokens in the accelerator.
  return base::UTF16ToUTF8(base::JoinString(hotkey_tokens, u"-"));
}

}  // namespace

std::string GetHotkeyString() {
  // No mapping used for base implementation.
  return GetHotkeyStringWithMapping(base::DoNothing());
}

#if BUILDFLAG(IS_MAC)
std::string GetLongFormMacHotkeyString() {
  return GetHotkeyStringWithMapping(
      base::BindRepeating([](std::u16string& token) {
        // Accelerator code returns hotkeys on Mac represented by their
        // respective symbols (i.e. ⌘) rather than their spelled forms (i.e.
        // Cmd). Map the former to the latter, as that is what is preferred by
        // the glic UI.
        if (token == u"⌃") {
          token = u"Ctrl";
        } else if (token == u"⌥") {
          token = u"Option";
        } else if (token == u"⇧") {
          token = u"Shift";
        } else if (token == u"⌘") {
          token = u"Cmd";
        }
      }));
}
#endif

}  // namespace glic
