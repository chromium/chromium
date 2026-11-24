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

namespace {

std::vector<std::string> SplitImeList(const std::string& ime_ids) {
  return base::SplitString(ime_ids, ",", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY);
}

}  // namespace

InputMethodPrefs::InputMethodPrefs(PrefService* prefs)
    : prefs_(CHECK_DEREF(prefs)) {}
InputMethodPrefs::~InputMethodPrefs() = default;

void InputMethodPrefs::UpdateEnabledImes(
    InputMethodDescriptors enabled_arc_imes) {
  std::vector<std::string> enabled_ime_list =
      SplitImeList(prefs_->GetString(ash::prefs::kLanguageEnabledImes));

  std::erase_if(enabled_ime_list, [](const auto& id) {
    return ash::extension_ime_util::IsArcIME(id);
  });
  for (const auto& descriptor : enabled_arc_imes)
    enabled_ime_list.push_back(descriptor.id());

  SetEnabledImes(enabled_ime_list);
}

void InputMethodPrefs::DisableIme(const std::string& ime_id) {
  std::vector<std::string> enabled_ime_list =
      SplitImeList(prefs_->GetString(ash::prefs::kLanguageEnabledImes));
  if (std::erase(enabled_ime_list, ime_id) == 0) {
    return;
  }
  SetEnabledImes(enabled_ime_list);
}

std::set<std::string> InputMethodPrefs::GetEnabledImes() const {
  const std::vector<std::string> imes =
      SplitImeList(prefs_->GetString(ash::prefs::kLanguageEnabledImes));
  return std::set<std::string>(imes.begin(), imes.end());
}

void InputMethodPrefs::SetEnabledImes(
    const std::vector<std::string>& enabled_ime_list) {
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

}  // namespace arc
