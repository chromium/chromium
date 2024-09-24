// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/desk_template.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/window_properties.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/sync_device_info/local_device_info_util.h"
#include "components/tab_groups/tab_group_info.h"
#include "ui/aura/window.h"

namespace ash {

namespace {

std::string TabGroupDataToString(const app_restore::RestoreData* restore_data) {
  std::string result = "tab groups:[";

  for (const auto& app : restore_data->app_id_to_launch_list()) {
    for (const auto& window : app.second) {
      for (const auto& tab_group :
           window.second->browser_extra_info.tab_group_infos) {
        result += "\n" + tab_group.ToString() + ",";
      }
    }
  }

  result += "]\n";
  return result;
}

}  // namespace

DeskTemplate::DeskTemplate(base::Uuid uuid,
                           DeskTemplateSource source,
                           const std::string& name,
                           const base::Time created_time,
                           DeskTemplateType type)
    : uuid_(std::move(uuid)),
      source_(source),
      type_(type),
      created_time_(created_time),
      template_name_(base::UTF8ToUTF16(name)),
      device_form_factor_(syncer::GetLocalDeviceFormFactor()) {}

DeskTemplate::DeskTemplate(base::Uuid uuid,
                           DeskTemplateSource source,
                           const std::string& name,
                           const base::Time created_time,
                           DeskTemplateType type,
                           bool should_launch_on_startup,
                           base::Value policy)
    : uuid_(std::move(uuid)),
      source_(source),
      type_(type),
      created_time_(created_time),
      template_name_(base::UTF8ToUTF16(name)),
      should_launch_on_startup_(should_launch_on_startup),
      device_form_factor_(syncer::GetLocalDeviceFormFactor()) {
  policy_definition_ = std::move(policy);
}

DeskTemplate::~DeskTemplate() = default;

// static
bool DeskTemplate::IsAppTypeSupported(aura::Window* window) {
  // For now we'll ignore crostini windows in desk templates.
  const chromeos::AppType app_type = window->GetProperty(chromeos::kAppTypeKey);
  switch (app_type) {
    case chromeos::AppType::NON_APP:
    case chromeos::AppType::CROSTINI_APP:
      return false;
    case chromeos::AppType::LACROS:
    case chromeos::AppType::ARC_APP:
    case chromeos::AppType::BROWSER:
    case chromeos::AppType::CHROME_APP:
    case chromeos::AppType::SYSTEM_APP:
      return true;
  }
}

constexpr char DeskTemplate::kIncognitoWindowIdentifier[];

std::unique_ptr<DeskTemplate> DeskTemplate::Clone() const {
  std::unique_ptr<DeskTemplate> desk_template = std::make_unique<DeskTemplate>(
      uuid_, source_, base::UTF16ToUTF8(template_name_), created_time_, type_);
  if (WasUpdatedSinceCreation())
    desk_template->set_updated_time(updated_time_);
  if (desk_restore_data_)
    desk_template->set_desk_restore_data(desk_restore_data_->Clone());
  desk_template->set_client_cache_guid(client_cache_guid_);
  desk_template->should_launch_on_startup_ = should_launch_on_startup_;
  desk_template->policy_definition_ = policy_definition_.Clone();
  desk_template->lacros_profile_id_ = lacros_profile_id_;
  return desk_template;
}

void DeskTemplate::SetDeskUuid(base::Uuid desk_uuid) {
  DCHECK(desk_restore_data_);
  desk_restore_data_->SetDeskUuid(desk_uuid);
}

std::string DeskTemplate::ToString() const {
  std::string result = GetDeskTemplateInfo(/*for_debugging=*/false);

  if (desk_restore_data_)
    result += desk_restore_data_->ToString();
  return result;
}

std::string DeskTemplate::ToDebugString() const {
  std::string result = GetDeskTemplateInfo(/*for_debugging=*/true);

  result += "Time created: " + base::TimeFormatHTTP(created_time_) + "\n";
  result += "Time updated: " + base::TimeFormatHTTP(updated_time_) + "\n";
  result += "auto launch: ";
  result += should_launch_on_startup_ ? "yes\n" : "no\n";
  result +=
      "Lacros profile ID: " + base::NumberToString(lacros_profile_id_) + "\n";

  // Converting to value and printing the debug string may be more
  // intensive but gives more complete information which increases
  // the utility of this function.
  if (desk_restore_data_) {
    result += desk_restore_data_->ConvertToValue().DebugString();
    result += TabGroupDataToString(desk_restore_data_.get());
  }
  return result;
}

std::string DeskTemplate::GetDeskTemplateInfo(bool for_debugging) const {
  std::string result =
      "Template name: " + base::UTF16ToUTF8(template_name_) + "\n";
  if (for_debugging)
    result += "GUID: " + uuid_.AsLowercaseString() + "\n";
  result += "Source: ";
  switch (source_) {
    case DeskTemplateSource::kUnknownSource:
      result += "unknown\n";
      break;
    case DeskTemplateSource::kUser:
      result += "user\n";
      break;
    case DeskTemplateSource::kPolicy:
      result += "policy\n";
      break;
  }
  result += "Type: ";
  switch (type_) {
    case DeskTemplateType::kTemplate:
      result += "template\n";
      break;
    case DeskTemplateType::kSaveAndRecall:
      result += "save and recall\n";
      break;
    case DeskTemplateType::kFloatingWorkspace:
      result += "floating workspace\n";
      break;
    case DeskTemplateType::kUnknown:
      result += "unknown\n";
      break;
  }
  return result;
}

}  // namespace ash
