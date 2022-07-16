// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/desk_template.h"

#include "base/strings/utf_string_conversions.h"

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

DeskTemplate::DeskTemplate()
    : uuid_(base::GUID::GenerateRandomV4()),
      source_(DeskTemplateSource::kUnknownSource),
      created_time_(base::Time::Now()) {}

std::unique_ptr<DeskTemplate> DeskTemplate::Clone() {
  std::unique_ptr<DeskTemplate> desk_template = std::make_unique<DeskTemplate>(
      uuid_.AsLowercaseString(), source_, base::UTF16ToUTF8(template_name_),
      created_time_);
  if (WasUpdatedSinceCreation())
    desk_template->set_updated_time(updated_time_);
  if (desk_restore_data_)
    desk_template->set_desk_restore_data(desk_restore_data_->Clone());
  return desk_template;
}

}  // namespace ash