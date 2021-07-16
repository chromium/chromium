// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/desk_template.h"

#include "base/strings/utf_string_conversions.h"

namespace ash {

DeskTemplate::DeskTemplate()
    : uuid_(base::GUID::GenerateRandomV4()), created_time_(base::Time::Now()) {}

DeskTemplate::DeskTemplate(const base::GUID& guid)
    : uuid_(guid), created_time_(base::Time::Now()) {}

DeskTemplate::DeskTemplate(const std::string& uuid,
                           const std::string& name,
                           const base::Time& created_time)
    : uuid_(base::GUID::ParseCaseInsensitive(uuid)),
      created_time_(created_time),
      template_name_(base::UTF8ToUTF16(name)) {}

DeskTemplate::~DeskTemplate() = default;

std::unique_ptr<DeskTemplate> DeskTemplate::Clone() {
  std::unique_ptr<DeskTemplate> desk_template = std::make_unique<DeskTemplate>(
      uuid_.AsLowercaseString(), base::UTF16ToUTF8(template_name_),
      created_time_);
  if (desk_restore_data_)
    desk_template->set_desk_restore_data(desk_restore_data_->Clone());
  return desk_template;
}

}  // namespace ash