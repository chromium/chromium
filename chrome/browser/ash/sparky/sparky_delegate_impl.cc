// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/sparky/sparky_delegate_impl.h"

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "ash/constants/ash_pref_names.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/manta/sparky/sparky_delegate.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/event_constants.h"

namespace ash {

namespace {
using SetPrefResult = extensions::settings_private::SetPrefResult;
using SettingsPrivatePrefType = extensions::api::settings_private::PrefType;
}  // namespace

SparkyDelegateImpl::SparkyDelegateImpl(Profile* profile)
    : profile_(profile),
      prefs_util_(std::make_unique<extensions::PrefsUtil>(profile)),
      screenshot_handler_(std::make_unique<sparky::ScreenshotHandler>()) {}

SparkyDelegateImpl::~SparkyDelegateImpl() = default;

bool SparkyDelegateImpl::SetSettings(
    std::unique_ptr<manta::SettingsData> settings_data) {
  if (!settings_data->val_set) {
    return false;
  }
  if (settings_data->pref_name == prefs::kDarkModeEnabled) {
    profile_->GetPrefs()->SetBoolean(settings_data->pref_name,
                                     settings_data->bool_val);
    return true;
  }

  SetPrefResult result = prefs_util_->SetPref(
      settings_data->pref_name, base::to_address(settings_data->GetValue()));

  return result == SetPrefResult::SUCCESS;
}

void SparkyDelegateImpl::AddPrefToMap(
    const std::string& pref_name,
    SettingsPrivatePrefType settings_pref_type,
    std::optional<base::Value> value) {
  // TODO (b:354608065) Add in UMA logging for these error cases.
  switch (settings_pref_type) {
    case SettingsPrivatePrefType::kBoolean: {
      if (!value->is_bool()) {
        DVLOG(1) << "Cros setting " << pref_name
                 << " has a prefType of bool, but has a value of type: "
                 << value->type();
        break;
      }
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
      } else {
        DVLOG(1) << "Cros setting " << pref_name
                 << " has a prefType of number, but has a value of type: "
                 << value->type();
      }
      break;
    }
    case SettingsPrivatePrefType::kList: {
      if (!value->is_list()) {
        DVLOG(1) << "Cros setting " << pref_name
                 << " has a prefType of list, but has a value of type: "
                 << value->type();
        break;
      }
      current_prefs_[pref_name] = std::make_unique<manta::SettingsData>(
          pref_name, manta::PrefType::kList, std::move(value));
      break;
    }
    case SettingsPrivatePrefType::kString:
    case SettingsPrivatePrefType::kUrl: {
      if (!value->is_string()) {
        DVLOG(1)
            << "Cros setting " << pref_name
            << " has a prefType of string or url, but has a value of type: "
            << value->type();
        break;
      }
      current_prefs_[pref_name] = std::make_unique<manta::SettingsData>(
          pref_name, manta::PrefType::kString, std::move(value));
      break;
    }
    case SettingsPrivatePrefType::kDictionary: {
      if (!value->is_dict()) {
        DVLOG(1) << "Cros setting " << pref_name
                 << " has a prefType of dictionary, but has a value of type: "
                 << value->type();
        break;
      }
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

void SparkyDelegateImpl::GetScreenshot(manta::ScreenshotDataCallback callback) {
  screenshot_handler_->TakeScreenshot(std::move(callback));
}

std::vector<manta::AppsData> SparkyDelegateImpl::GetAppsList() {
  std::vector<manta::AppsData> apps;
  apps::AppServiceProxyFactory::GetForProfile(profile_)
      ->AppRegistryCache()
      .ForEachApp([&apps](const apps::AppUpdate& update) {
        if (!apps_util::IsInstalled(update.Readiness())) {
          return;
        }

        if (!update.ShowInSearch().value_or(false) &&
            !(update.Recommendable().value_or(false) &&
              update.AppType() == apps::AppType::kBuiltIn)) {
          return;
        }

        manta::AppsData& app = apps.emplace_back(update.AppId(), update.Name());

        for (const std::string& term : update.AdditionalSearchTerms()) {
          app.AddSearchableText(term);
        }
      });
  return apps;
}

void SparkyDelegateImpl::LaunchApp(const std::string& app_id) {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile_);
  proxy->Launch(app_id, ui::EF_IS_SYNTHESIZED, apps::LaunchSource::kFromSparky,
                std::make_unique<apps::WindowInfo>(display::kDefaultDisplayId));
}

}  // namespace ash
