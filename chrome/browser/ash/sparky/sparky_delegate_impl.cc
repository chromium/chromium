// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/sparky/sparky_delegate_impl.h"

#include <map>
#include <memory>
#include <optional>

#include "ash/constants/ash_pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "components/manta/sparky/sparky_delegate.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace {
using SetPrefResult = extensions::settings_private::SetPrefResult;
using SettingsPrivatePrefType = extensions::api::settings_private::PrefType;
}  // namespace

SparkyDelegateImpl::SparkyDelegateImpl(Profile* profile)
    : profile_(profile),
      prefs_util_(std::make_unique<extensions::PrefsUtil>(profile)) {}

SparkyDelegateImpl::~SparkyDelegateImpl() = default;

bool SparkyDelegateImpl::SetSettings(
    std::unique_ptr<manta::SettingsData> settings_data) {
  if (settings_data->pref_name == prefs::kDarkModeEnabled) {
    profile_->GetPrefs()->SetBoolean(settings_data->pref_name,
                                     settings_data->value->GetBool());
    return true;
  }

  SetPrefResult result = prefs_util_->SetPref(
      settings_data->pref_name, base::to_address(settings_data->value));

  return result == SetPrefResult::SUCCESS;
}

void SparkyDelegateImpl::AddPrefToMap(
    const std::string& pref_name,
    SettingsPrivatePrefType settings_pref_type,
    std::optional<base::Value> value) {
  switch (settings_pref_type) {
    case SettingsPrivatePrefType::kBoolean: {
      current_prefs_[pref_name] = std::make_unique<manta::SettingsData>(
          pref_name, manta::PrefType::kBoolean, std::move(value));
      break;
    }
    case SettingsPrivatePrefType::kNumber: {
      if (value->is_int()) {
        current_prefs_[pref_name] = std::make_unique<manta::SettingsData>(
            pref_name, manta::PrefType::kInt, std::move(value));
      } else if (value->is_double()) {
        current_prefs_[pref_name] = std::make_unique<manta::SettingsData>(
            pref_name, manta::PrefType::kDouble, std::move(value));
      }
      break;
    }
    case SettingsPrivatePrefType::kList: {
      current_prefs_[pref_name] = std::make_unique<manta::SettingsData>(
          pref_name, manta::PrefType::kList, std::move(value));
      break;
    }
    case SettingsPrivatePrefType::kString:
    case SettingsPrivatePrefType::kUrl: {
      current_prefs_[pref_name] = std::make_unique<manta::SettingsData>(
          pref_name, manta::PrefType::kString, std::move(value));
      break;
    }
    case SettingsPrivatePrefType::kDictionary: {
      current_prefs_[pref_name] = std::make_unique<manta::SettingsData>(
          pref_name, manta::PrefType::kDictionary, std::move(value));
      break;
    }
    default:
      break;
  }
}

SparkyDelegateImpl::SettingsDataList* SparkyDelegateImpl::GetSettingsList() {
  extensions::PrefsUtil::TypedPrefMap pref_list =
      prefs_util_->GetAllowlistedKeys();

  current_prefs_ = SparkyDelegateImpl::SettingsDataList();

  for (auto const& [pref_name, pref_type] : pref_list) {
    auto pref_object = prefs_util_->GetPref(pref_name);
    if (pref_object.has_value()) {
      AddPrefToMap(pref_name, pref_type, std::move(pref_object->value));
    }
  }

  current_prefs_[prefs::kDarkModeEnabled] =
      std::make_unique<manta::SettingsData>(
          prefs::kDarkModeEnabled, manta::PrefType::kBoolean,
          std::make_optional<base::Value>(
              profile_->GetPrefs()->GetBoolean(prefs::kDarkModeEnabled)));
  return &current_prefs_;
}

std::optional<base::Value> SparkyDelegateImpl::GetSettingValue(
    const std::string& setting_id) {
  if (setting_id == prefs::kDarkModeEnabled) {
    return std::make_optional<base::Value>(
        profile_->GetPrefs()->GetBoolean(prefs::kDarkModeEnabled));
  }
  auto pref_object = prefs_util_->GetPref(setting_id);
  if (pref_object.has_value()) {
    return std::move(pref_object->value);
  } else {
    return std::nullopt;
  }
}

}  // namespace ash
