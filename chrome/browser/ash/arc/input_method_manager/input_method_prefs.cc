// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_method_manager/input_method_prefs.h"

#include <algorithm>
#include <vector>

#include "ash/constants/ash_pref_names.h"
#include "base/check_deref.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/prefs/pref_service.h"
#include "ui/base/ime/ash/extension_ime_util.h"

namespace arc {

namespace ce = ::ash::extension_ime_util;
using ::ash::input_method::InputMethodDescriptors;

InputMethodPrefs::InputMethodPrefs(PrefService* prefs)
    : prefs_(CHECK_DEREF(prefs)) {}
InputMethodPrefs::~InputMethodPrefs() = default;

void InputMethodPrefs::UpdateEnabledImes(
    InputMethodDescriptors enabled_arc_imes) {
  const std::string enabled_ime_ids =
      prefs_->GetString(ash::prefs::kLanguageEnabledImes);
  std::vector<std::string> enabled_ime_list = base::SplitString(
      enabled_ime_ids, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  std::erase_if(enabled_ime_list, [](const auto& id) {
    return ash::extension_ime_util::IsArcIME(id);
  });
  for (const auto& descriptor : enabled_arc_imes)
    enabled_ime_list.push_back(descriptor.id());

  prefs_->SetString(ash::prefs::kLanguageEnabledImes,
                    base::JoinString(enabled_ime_list, ","));

  const std::string current_ime =
      prefs_->GetString(ash::prefs::kLanguageCurrentInputMethod);
  if (ce::IsArcIME(current_ime) &&
      !std::ranges::contains(enabled_ime_list, current_ime)) {
    prefs_->SetString(ash::prefs::kLanguageCurrentInputMethod, std::string());
  }
  const std::string previous_ime =
      prefs_->GetString(ash::prefs::kLanguagePreviousInputMethod);
  if (ce::IsArcIME(previous_ime) &&
      !std::ranges::contains(enabled_ime_list, previous_ime)) {
    prefs_->SetString(ash::prefs::kLanguagePreviousInputMethod, std::string());
  }
}

std::set<std::string> InputMethodPrefs::GetEnabledImes() const {
  const std::vector<std::string> imes =
      base::SplitString(prefs_->GetString(ash::prefs::kLanguageEnabledImes),
                        ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  return std::set<std::string>(imes.begin(), imes.end());
}

}  // namespace arc
