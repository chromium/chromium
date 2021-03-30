// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_preview_sticky_settings.h"

#include "base/json/json_reader.h"
#include "base/no_destructor.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

namespace printing {

namespace {

constexpr char kId[] = "id";
constexpr char kRecentDestinations[] = "recentDestinations";
constexpr char kSettingAppState[] = "appState";

}  // namespace

// static
PrintPreviewStickySettings* PrintPreviewStickySettings::GetInstance() {
  static base::NoDestructor<PrintPreviewStickySettings> instance;
  return instance.get();
}

PrintPreviewStickySettings::PrintPreviewStickySettings() = default;

PrintPreviewStickySettings::~PrintPreviewStickySettings() = default;

const std::string* PrintPreviewStickySettings::printer_app_state() const {
  return printer_app_state_ ? &printer_app_state_.value() : nullptr;
}

void PrintPreviewStickySettings::StoreAppState(const std::string& data) {
  printer_app_state_ = base::make_optional(data);
}

void PrintPreviewStickySettings::SaveInPrefs(PrefService* prefs) const {
  base::Value dict(base::Value::Type::DICTIONARY);
  if (printer_app_state_)
    dict.SetKey(kSettingAppState, base::Value(*printer_app_state_));
  prefs->Set(prefs::kPrintPreviewStickySettings, dict);
}

void PrintPreviewStickySettings::RestoreFromPrefs(PrefService* prefs) {
  const base::DictionaryValue* value =
      prefs->GetDictionary(prefs::kPrintPreviewStickySettings);
  const base::Value* app_state =
      value->FindKeyOfType(kSettingAppState, base::Value::Type::STRING);
  if (app_state)
    StoreAppState(app_state->GetString());
}

base::flat_map<std::string, int>
PrintPreviewStickySettings::GetPrinterRecentlyUsedRanks() {
  auto recently_used_printers = GetRecentlyUsedPrinters();
  int current_rank = 0;
  std::vector<std::pair<std::string, int>> recently_used_ranks;
  recently_used_ranks.reserve(recently_used_printers.size());
  for (std::string& printer_id : recently_used_printers)
    recently_used_ranks.emplace_back(std::move(printer_id), current_rank++);
  return recently_used_ranks;
}

std::vector<std::string> PrintPreviewStickySettings::GetRecentlyUsedPrinters() {
  const std::string* sticky_settings_state = printer_app_state();
  if (!sticky_settings_state)
    return {};

  base::Optional<base::Value> sticky_settings_state_value =
      base::JSONReader::Read(*sticky_settings_state);
  if (!sticky_settings_state_value || !sticky_settings_state_value->is_dict())
    return {};

  base::Value* recent_destinations =
      sticky_settings_state_value->FindListKey(kRecentDestinations);
  if (!recent_destinations)
    return {};

  std::vector<std::string> printers;
  printers.reserve(recent_destinations->GetList().size());
  for (const auto& recent_destination : recent_destinations->GetList()) {
    const std::string* printer_id = recent_destination.FindStringKey(kId);
    if (!printer_id)
      continue;
    printers.push_back(*printer_id);
  }
  return printers;
}

// static
void PrintPreviewStickySettings::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kPrintPreviewStickySettings);
}

}  // namespace printing
