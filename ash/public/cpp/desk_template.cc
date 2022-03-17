// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/desk_template.h"

#include "ash/constants/app_types.h"
#include "ash/constants/ash_features.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"

namespace ash {

DeskTemplate::DeskTemplate(const std::string& uuid,
                           DeskTemplateSource source,
                           const std::string& name,
                           const base::Time created_time)
    : uuid_(base::GUID::ParseCaseInsensitive(uuid)),
      source_(source),
      created_time_(created_time),
      template_name_(base::UTF8ToUTF16(name)) {}

DeskTemplate::~DeskTemplate() = default;

// static
bool DeskTemplate::IsAppTypeSupported(aura::Window* window) {
  // For now we'll ignore crostini and lacros windows in desk template. We'll
  // also ignore ARC apps unless the flag is turned on.
  const AppType app_type =
      static_cast<AppType>(window->GetProperty(aura::client::kAppType));
  switch (app_type) {
    case AppType::NON_APP:
    case AppType::CROSTINI_APP:
    case AppType::LACROS:
      return false;
    case AppType::ARC_APP:
      if (!features::AreDesksTemplatesEnabled())
        return false;
      break;
    case AppType::BROWSER:
    case AppType::CHROME_APP:
    case AppType::SYSTEM_APP:
      break;
  }

  return true;
}

constexpr char DeskTemplate::kIncognitoWindowIdentifier[];

DeskTemplate::DeskTemplate()
    : uuid_(base::GUID::GenerateRandomV4()),
      source_(DeskTemplateSource::kUnknownSource),
      created_time_(base::Time::Now()) {}

std::unique_ptr<DeskTemplate> DeskTemplate::Clone() const {
  std::unique_ptr<DeskTemplate> desk_template = std::make_unique<DeskTemplate>(
      uuid_.AsLowercaseString(), source_, base::UTF16ToUTF8(template_name_),
      created_time_);
  if (WasUpdatedSinceCreation())
    desk_template->set_updated_time(updated_time_);
  if (desk_restore_data_)
    desk_template->set_desk_restore_data(desk_restore_data_->Clone());
  desk_template->set_launch_id(launch_id_);
  return desk_template;
}

void DeskTemplate::SetDeskIndex(int desk_index) {
  DCHECK(desk_restore_data_);
  desk_restore_data_->SetDeskIndex(desk_index);
}

std::string DeskTemplate::ToString() const {
  std::string result =
      "Template name: " + base::UTF16ToUTF8(template_name_) + "\n";
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

  if (desk_restore_data_)
    result += desk_restore_data_->ToString();
  return result;
}

}  // namespace ash
