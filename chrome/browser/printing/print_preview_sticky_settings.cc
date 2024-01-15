// Copyright 2012 The Chromium Authors
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
  printer_app_state_ = std::make_optional(data);
}

void PrintPreviewStickySettings::SaveInPrefs(PrefService* prefs) const {
  base::Value::Dict dict;
  if (printer_app_state_)
    dict.Set(kSettingAppState, *printer_app_state_);
  prefs->SetDict(prefs::kPrintPreviewStickySettings, std::move(dict));
}

void PrintPreviewStickySettings::RestoreFromPrefs(PrefService* prefs) {
  const base::Value::Dict& value =
      prefs->GetDict(prefs::kPrintPreviewStickySettings);
  const std::string* app_state = value.FindString(kSettingAppState);
  if (app_state)
    StoreAppState(*app_state);
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

  std::optional<base::Value> sticky_settings_state_value =
      base::JSONReader::Read(*sticky_settings_state);
  if (!sticky_settings_state_value || !sticky_settings_state_value->is_dict())
    return {};

  base::Value::List* recent_destinations =
      sticky_settings_state_value->GetDict().FindList(kRecentDestinations);
  if (!recent_destinations)
    return {};

  std::vector<std::string> printers;
  printers.reserve(recent_destinations->size());
  for (const auto& recent_destination : *recent_destinations) {
    if (!recent_destination.is_dict())
      continue;
    const std::string* printer_id =
        recent_destination.GetDict().FindString(kId);
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
