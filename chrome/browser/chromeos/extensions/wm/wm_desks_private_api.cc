// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/wm/wm_desks_private_api.h"

#include <memory>

#include "ash/public/cpp/desk_template.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/desks_templates/desks_templates_client.h"
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

}  // namespace

WmDesksPrivateCaptureActiveDeskAndSaveTemplateFunction::
    WmDesksPrivateCaptureActiveDeskAndSaveTemplateFunction() = default;
WmDesksPrivateCaptureActiveDeskAndSaveTemplateFunction::
    ~WmDesksPrivateCaptureActiveDeskAndSaveTemplateFunction() = default;

ExtensionFunction::ResponseAction
WmDesksPrivateCaptureActiveDeskAndSaveTemplateFunction::Run() {
  DesksTemplatesClient::Get()->CaptureActiveDeskAndSaveTemplate(
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

  DesksTemplatesClient::Get()->UpdateDeskTemplate(
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
  DesksTemplatesClient::Get()->GetDeskTemplates(base::BindOnce(
      &WmDesksPrivateGetSavedDeskTemplatesFunction::OnGetSavedDeskTemplate,
      this));
  return RespondLater();
}

void WmDesksPrivateGetSavedDeskTemplatesFunction::OnGetSavedDeskTemplate(
    const std::vector<ash::DeskTemplate*>& desk_templates,
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

  DesksTemplatesClient::Get()->GetTemplateJson(
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

  DesksTemplatesClient::Get()->DeleteDeskTemplate(
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

WmDesksPrivateLaunchDeskTemplateFunction::
    WmDesksPrivateLaunchDeskTemplateFunction() = default;
WmDesksPrivateLaunchDeskTemplateFunction::
    ~WmDesksPrivateLaunchDeskTemplateFunction() = default;

ExtensionFunction::ResponseAction
WmDesksPrivateLaunchDeskTemplateFunction::Run() {
  std::unique_ptr<api::wm_desks_private::LaunchDeskTemplate::Params> params(
      api::wm_desks_private::LaunchDeskTemplate::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  DesksTemplatesClient::Get()->LaunchDeskTemplate(
      params->template_uuid,
      base::BindOnce(
          &WmDesksPrivateLaunchDeskTemplateFunction::OnLaunchDeskTemplate,
          this));
  return RespondLater();
}

void WmDesksPrivateLaunchDeskTemplateFunction::OnLaunchDeskTemplate(
    std::string error_string) {
  if (!error_string.empty()) {
    Respond(Error(std::move(error_string)));
    return;
  }

  Respond(NoArguments());
}

}  // namespace extensions
