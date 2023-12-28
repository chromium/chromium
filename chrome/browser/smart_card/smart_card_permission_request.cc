// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/smart_card/smart_card_permission_request.h"
#include "base/strings/utf_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

SmartCardPermissionRequest::SmartCardPermissionRequest(
    const url::Origin& requesting_origin,
    const std::string& reader_name,
    ResultCallback result_callback)
    : permissions::PermissionRequest(
          requesting_origin.GetURL(),
          permissions::RequestType::kSmartCard,
          /*has_gesture=*/false,
          base::BindRepeating(&SmartCardPermissionRequest::OnPermissionDecided,
                              base::Unretained(this)),
          base::BindOnce(&SmartCardPermissionRequest::DeleteRequest,
                         base::Unretained(this))),
      reader_name_(reader_name),
      result_callback_(std::move(result_callback)) {}

SmartCardPermissionRequest::~SmartCardPermissionRequest() = default;

bool SmartCardPermissionRequest::IsDuplicateOf(
    permissions::PermissionRequest* other_request) const {
  // The downcast here is safe because PermissionRequest::IsDuplicateOf ensures
  // that both requests are of type RequestType::kSmartCard.
  return permissions::PermissionRequest::IsDuplicateOf(other_request) &&
         reader_name_ == static_cast<SmartCardPermissionRequest*>(other_request)
                             ->reader_name_;
}

std::u16string SmartCardPermissionRequest::GetMessageTextFragment() const {
  return l10n_util::GetStringFUTF16(IDS_SMART_CARD_PERMISSION_PROMPT,
                                    base::ASCIIToUTF16(reader_name_));
}

std::optional<std::u16string> SmartCardPermissionRequest::GetAllowAlwaysText()
    const {
  return l10n_util::GetStringUTF16(IDS_SMART_CARD_PERMISSION_ALWAYS_ALLOW);
}

void SmartCardPermissionRequest::OnPermissionDecided(
    ContentSetting content_setting_result,
    bool is_one_time,
    bool is_final_decision) {
  if (!is_final_decision) {
    return;
  }

  Result result = Result::kDontAllow;

  if (content_setting_result == ContentSetting::CONTENT_SETTING_ALLOW) {
    if (is_one_time) {
      result = Result::kAllowOnce;
    } else {
      result = Result::kAllowAlways;
    }
  }

  std::move(result_callback_).Run(result);
}

void SmartCardPermissionRequest::DeleteRequest() {
  delete this;
}
