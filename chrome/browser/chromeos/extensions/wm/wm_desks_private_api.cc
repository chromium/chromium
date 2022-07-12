// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/wm/wm_desks_private_api.h"

#include <memory>

#include "ash/public/cpp/desk_template.h"
#include "ash/wm/desks/desk.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/desks/desks_client.h"
#include "chrome/common/extensions/api/wm_desks_private.h"

namespace extensions {

namespace {

api::wm_desks_private::DeskTemplate FromAshDeskTemplate(
    const ash::DeskTemplate& desk_template) {
  api::wm_desks_private::DeskTemplate out_api_template;
  out_api_template.template_uuid = desk_template.uuid().AsLowercaseString();
  out_api_template.template_name =
      base::UTF16ToUTF8(desk_template.template_name());
  return out_api_template;
}

api::wm_desks_private::Desk FromAshDesk(const ash::Desk& ash_desk) {
  api::wm_desks_private::Desk target;
  target.desk_name = base::UTF16ToUTF8(ash_desk.name());
  target.desk_uuid = ash_desk.uuid().AsLowercaseString();
  return target;
}

}  // namespace

WmDesksPrivateCaptureActiveDeskAndSaveTemplateFunction::
    WmDesksPrivateCaptureActiveDeskAndSaveTemplateFunction() = default;
WmDesksPrivateCaptureActiveDeskAndSaveTemplateFunction::
    ~WmDesksPrivateCaptureActiveDeskAndSaveTemplateFunction() = default;

ExtensionFunction::ResponseAction
WmDesksPrivateCaptureActiveDeskAndSaveTemplateFunction::Run() {
  DesksClient::Get()->CaptureActiveDeskAndSaveTemplate(
      base::BindOnce(&WmDesksPrivateCaptureActiveDeskAndSaveTemplateFunction::
                         OnCaptureActiveDeskAndSaveTemplateCompleted,
                     this));
  return RespondLater();
}

void WmDesksPrivateCaptureActiveDeskAndSaveTemplateFunction::
    OnCaptureActiveDeskAndSaveTemplateCompleted(
        std::unique_ptr<ash::DeskTemplate> desk_template,
        std::string error_string) {
  if (!error_string.empty()) {
    Respond(Error(std::move(error_string)));
    return;
  }

  api::wm_desks_private::DeskTemplate api_template =
      FromAshDeskTemplate(*desk_template);
  Respond(ArgumentList(
      api::wm_desks_private::CaptureActiveDeskAndSaveTemplate::Results::Create(
          api_template)));
}

WmDesksPrivateUpdateDeskTemplateFunction::
    WmDesksPrivateUpdateDeskTemplateFunction() = default;
WmDesksPrivateUpdateDeskTemplateFunction::
    ~WmDesksPrivateUpdateDeskTemplateFunction() = default;

ExtensionFunction::ResponseAction
WmDesksPrivateUpdateDeskTemplateFunction::Run() {
  std::unique_ptr<api::wm_desks_private::UpdateDeskTemplate::Params> params(
      api::wm_desks_private::UpdateDeskTemplate::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  DesksClient::Get()->UpdateDeskTemplate(
      params->desk_template.template_uuid,
      base::UTF8ToUTF16(params->desk_template.template_name),
      base::BindOnce(&WmDesksPrivateUpdateDeskTemplateFunction::
                         OnUpdateDeskTemplateCompleted,
                     this));
  return RespondLater();
}

void WmDesksPrivateUpdateDeskTemplateFunction::OnUpdateDeskTemplateCompleted(
    std::string error_string) {
  if (!error_string.empty()) {
    Respond(Error(std::move(error_string)));
    return;
  }

  Respond(NoArguments());
}

WmDesksPrivateGetSavedDeskTemplatesFunction::
    WmDesksPrivateGetSavedDeskTemplatesFunction() = default;
WmDesksPrivateGetSavedDeskTemplatesFunction::
    ~WmDesksPrivateGetSavedDeskTemplatesFunction() = default;

ExtensionFunction::ResponseAction
WmDesksPrivateGetSavedDeskTemplatesFunction::Run() {
  DesksClient::Get()->GetDeskTemplates(base::BindOnce(
      &WmDesksPrivateGetSavedDeskTemplatesFunction::OnGetSavedDeskTemplate,
      this));
  return RespondLater();
}

void WmDesksPrivateGetSavedDeskTemplatesFunction::OnGetSavedDeskTemplate(
    const std::vector<const ash::DeskTemplate*>& desk_templates,
    std::string error_string) {
  if (!error_string.empty()) {
    Respond(Error(std::move(error_string)));
    return;
  }

  // Construct the value.
  std::vector<api::wm_desks_private::DeskTemplate> api_templates;
  for (auto* desk_template : desk_templates) {
    api::wm_desks_private::DeskTemplate api_template =
        FromAshDeskTemplate(*desk_template);
    api_templates.push_back(std::move(api_template));
  }

  Respond(ArgumentList(
      api::wm_desks_private::GetSavedDeskTemplates::Results::Create(
          api_templates)));
}

WmDesksPrivateGetDeskTemplateJsonFunction::
    WmDesksPrivateGetDeskTemplateJsonFunction() = default;
WmDesksPrivateGetDeskTemplateJsonFunction::
    ~WmDesksPrivateGetDeskTemplateJsonFunction() = default;

ExtensionFunction::ResponseAction
WmDesksPrivateGetDeskTemplateJsonFunction::Run() {
  std::unique_ptr<api::wm_desks_private::GetDeskTemplateJson::Params> params(
      api::wm_desks_private::GetDeskTemplateJson::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  DesksClient::Get()->GetTemplateJson(
      params->template_uuid, Profile::FromBrowserContext(browser_context()),
      base::BindOnce(
          &WmDesksPrivateGetDeskTemplateJsonFunction::OnGetDeskTemplateJson,
          this));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void WmDesksPrivateGetDeskTemplateJsonFunction::OnGetDeskTemplateJson(
    const std::string& template_json,
    std::string error_string) {
  if (!error_string.empty()) {
    Respond(Error(std::move(error_string)));
    return;
  }

  Respond(
      ArgumentList(api::wm_desks_private::GetDeskTemplateJson::Results::Create(
          template_json)));
}

WmDesksPrivateDeleteDeskTemplateFunction::
    WmDesksPrivateDeleteDeskTemplateFunction() = default;
WmDesksPrivateDeleteDeskTemplateFunction::
    ~WmDesksPrivateDeleteDeskTemplateFunction() = default;

ExtensionFunction::ResponseAction
WmDesksPrivateDeleteDeskTemplateFunction::Run() {
  std::unique_ptr<api::wm_desks_private::DeleteDeskTemplate::Params> params(
      api::wm_desks_private::DeleteDeskTemplate::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  DesksClient::Get()->DeleteDeskTemplate(
      params->template_uuid,
      base::BindOnce(&WmDesksPrivateDeleteDeskTemplateFunction::
                         OnDeleteDeskTemplateCompleted,
                     this));
  return RespondLater();
}

void WmDesksPrivateDeleteDeskTemplateFunction::OnDeleteDeskTemplateCompleted(
    std::string error_string) {
  if (!error_string.empty()) {
    Respond(Error(std::move(error_string)));
    return;
  }

  Respond(NoArguments());
}

WmDesksPrivateLaunchDeskFunction::WmDesksPrivateLaunchDeskFunction() = default;
WmDesksPrivateLaunchDeskFunction::~WmDesksPrivateLaunchDeskFunction() = default;

ExtensionFunction::ResponseAction WmDesksPrivateLaunchDeskFunction::Run() {
  std::unique_ptr<api::wm_desks_private::LaunchDesk::Params> params(
      api::wm_desks_private::LaunchDesk::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);
  auto& launch_options = params->launch_options;
  std::u16string desk_name = launch_options.desk_name
                                 ? base::UTF8ToUTF16(*launch_options.desk_name)
                                 : u"";
  if (launch_options.template_uuid) {
    DesksClient::Get()->LaunchDeskTemplate(
        *params->launch_options.template_uuid,
        base::BindOnce(&WmDesksPrivateLaunchDeskFunction::OnLaunchDesk, this),
        desk_name);
  } else {
    DesksClient::Get()->LaunchEmptyDesk(
        base::BindOnce(&WmDesksPrivateLaunchDeskFunction::OnLaunchDesk, this),
        desk_name);
  }
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void WmDesksPrivateLaunchDeskFunction::OnLaunchDesk(
    std::string error_string,
    const base::GUID& desk_uuid) {
  if (!error_string.empty()) {
    Respond(Error(std::move(error_string)));
    return;
  }

  Respond(ArgumentList(api::wm_desks_private::LaunchDesk::Results::Create(
      desk_uuid.AsLowercaseString())));
}

WmDesksPrivateRemoveDeskFunction::WmDesksPrivateRemoveDeskFunction() = default;
WmDesksPrivateRemoveDeskFunction::~WmDesksPrivateRemoveDeskFunction() = default;

ExtensionFunction::ResponseAction WmDesksPrivateRemoveDeskFunction::Run() {
  std::unique_ptr<api::wm_desks_private::RemoveDesk::Params> params(
      api::wm_desks_private::RemoveDesk::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);
  DesksClient::Get()->RemoveDesk(
      base::GUID::ParseCaseInsensitive(params->desk_id),
      params->remove_desk_options ? params->remove_desk_options->combine_desks
                                  : false,
      base::BindOnce(&WmDesksPrivateRemoveDeskFunction::OnRemoveDesk, this));
  return RespondLater();
}

void WmDesksPrivateRemoveDeskFunction::OnRemoveDesk(std::string error_string) {
  if (!error_string.empty()) {
    Respond(Error(std::move(error_string)));
    return;
  }

  Respond(NoArguments());
}

WmDesksPrivateGetAllDesksFunction::WmDesksPrivateGetAllDesksFunction() =
    default;
WmDesksPrivateGetAllDesksFunction::~WmDesksPrivateGetAllDesksFunction() =
    default;

ExtensionFunction::ResponseAction WmDesksPrivateGetAllDesksFunction::Run() {
  DesksClient::Get()->GetAllDesks(
      base::BindOnce(&WmDesksPrivateGetAllDesksFunction::OnGetAllDesks, this));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void WmDesksPrivateGetAllDesksFunction::OnGetAllDesks(
    const std::vector<const ash::Desk*>& desks,
    std::string error_string) {
  if (!error_string.empty()) {
    Respond(Error(std::move(error_string)));
    return;
  }

  std::vector<api::wm_desks_private::Desk> api_desks;
  for (const ash::Desk* desk : desks)
    api_desks.push_back(FromAshDesk(*desk));

  Respond(ArgumentList(
      api::wm_desks_private::GetAllDesks::Results::Create(api_desks)));
}

WmDesksPrivateSetWindowPropertiesFunction::
    WmDesksPrivateSetWindowPropertiesFunction() = default;
WmDesksPrivateSetWindowPropertiesFunction::
    ~WmDesksPrivateSetWindowPropertiesFunction() = default;

ExtensionFunction::ResponseAction
WmDesksPrivateSetWindowPropertiesFunction::Run() {
  std::unique_ptr<api::wm_desks_private::SetWindowProperties::Params> params(
      api::wm_desks_private::SetWindowProperties::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);
  DesksClient::Get()->SetAllDeskPropertyByBrowserSessionId(
      SessionID::FromSerializedValue(params->window_id),
      params->window_properties.all_desks,
      base::BindOnce(
          &WmDesksPrivateSetWindowPropertiesFunction::OnSetWindowProperties,
          this));
  return RespondLater();
}

void WmDesksPrivateSetWindowPropertiesFunction::OnSetWindowProperties(
    std::string error_string) {
  if (!error_string.empty()) {
    Respond(Error(std::move(error_string)));
    return;
  }
  Respond(NoArguments());
}

}  // namespace extensions
