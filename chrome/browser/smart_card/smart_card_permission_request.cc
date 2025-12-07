// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/smart_card/smart_card_permission_request.h"

#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_decision.h"
#include "components/permissions/permission_request_data.h"
#include "components/permissions/request_type.h"
#include "components/permissions/resolvers/content_setting_permission_resolver.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

SmartCardPermissionRequest::SmartCardPermissionRequest(
    const url::Origin& requesting_origin,
    const std::string& reader_name,
    ResultCallback result_callback)
    : permissions::PermissionRequest(
          std::make_unique<permissions::PermissionRequestData>(
              std::make_unique<permissions::ContentSettingPermissionResolver>(
                  ContentSettingsType::SMART_CARD_DATA),
              /*user_gesture=*/false,
              requesting_origin.GetURL()),
          base::BindRepeating(&SmartCardPermissionRequest::OnPermissionDecided,
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

std::optional<std::u16string> SmartCardPermissionRequest::GetBlockText() const {
  return l10n_util::GetStringUTF16(IDS_PERMISSION_DONT_ALLOW);
}

void SmartCardPermissionRequest::OnPermissionDecided(
    PermissionDecision decision,
    bool is_final_decision,
    const permissions::PermissionRequestData& request_data) {
  if (!is_final_decision) {
    return;
  }

  std::move(result_callback_).Run(decision);
}
