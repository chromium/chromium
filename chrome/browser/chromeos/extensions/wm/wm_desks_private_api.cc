// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/wm/wm_desks_private_api.h"

#include <memory>

#include "ash/public/cpp/desk_template.h"
#include "ash/public/cpp/desks_helper.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/ui/ash/desks_client.h"
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
  DesksClient::Get()->CaptureActiveDeskAndSaveTemplate(
      base::BindOnce(&WmDesksPrivateCaptureActiveDeskAndSaveTemplateFunction::
                         OnCaptureActiveDeskAndSaveTemplateCompleted,
                     this));
  return RespondLater();
}

void WmDesksPrivateCaptureActiveDeskAndSaveTemplateFunction::
    OnCaptureActiveDeskAndSaveTemplateCompleted(
        bool success,
        std::unique_ptr<ash::DeskTemplate> desk_template) {
  if (!success) {
    Respond(Error("Can't capture the active desk and save it as a template!"));
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
      api::wm_desks_private::UpdateDeskTemplate::Params::Create(*args_));
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
    bool success) {
  if (!success) {
    Respond(Error("Can't update the template!"));
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
    bool success,
    const std::vector<ash::DeskTemplate*>& desk_templates) {
  if (!success) {
    Respond(Error("Can't get the template list!"));
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

WmDesksPrivateDeleteDeskTemplateFunction::
    WmDesksPrivateDeleteDeskTemplateFunction() = default;
WmDesksPrivateDeleteDeskTemplateFunction::
    ~WmDesksPrivateDeleteDeskTemplateFunction() = default;

ExtensionFunction::ResponseAction
WmDesksPrivateDeleteDeskTemplateFunction::Run() {
  std::unique_ptr<api::wm_desks_private::DeleteDeskTemplate::Params> params(
      api::wm_desks_private::DeleteDeskTemplate::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  DesksClient::Get()->DeleteDeskTemplate(
      params->template_uuid,
      base::BindOnce(&WmDesksPrivateDeleteDeskTemplateFunction::
                         OnDeleteDeskTemplateCompleted,
                     this));
  return RespondLater();
}

void WmDesksPrivateDeleteDeskTemplateFunction::OnDeleteDeskTemplateCompleted(
    bool success) {
  if (!success) {
    Respond(Error("Can't delete the template!"));
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
      api::wm_desks_private::LaunchDeskTemplate::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  DesksClient::Get()->LaunchDeskTemplate(
      params->template_uuid,
      base::BindOnce(
          &WmDesksPrivateLaunchDeskTemplateFunction::OnLaunchDeskTemplate,
          this));
  return RespondLater();
}

void WmDesksPrivateLaunchDeskTemplateFunction::OnLaunchDeskTemplate(
    bool success) {
  if (!success) {
    Respond(Error("Can't launch the template!"));
    return;
  }

  Respond(NoArguments());
}

}  // namespace extensions
