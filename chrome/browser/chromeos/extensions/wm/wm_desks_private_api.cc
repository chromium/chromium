// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/wm/wm_desks_private_api.h"

#include <memory>

#include "ash/public/cpp/desk_template.h"
#include "ash/wm/desks/desk.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/desks/desks_client.h"
#include "chrome/common/extensions/api/wm_desks_private.h"

namespace extensions {

namespace {

constexpr char kInvalidUuidError[] = "Invalid template UUID.";
constexpr char kInvalidDeskIdError[] = "The desk identifier is not valid.";
constexpr char kApiLaunchDeskResult[] = "Ash.DeskApi.LaunchDesk.Result";
constexpr char kApiRemoveDeskResult[] = "Ash.DeskApi.RemoveDesk.Result";
constexpr char kApiSwitchDeskResult[] = "Ash.DeskApi.SwitchDesk.Result";
constexpr char kApiAllDeskResult[] = "Ash.DeskApi.AllDesk.Result";

api::wm_desks_private::Desk FromAshDesk(const ash::Desk& ash_desk) {
  api::wm_desks_private::Desk target;
  target.desk_name = base::UTF16ToUTF8(ash_desk.name());
  target.desk_uuid = ash_desk.uuid().AsLowercaseString();
  return target;
}

api::wm_desks_private::Desk GetSavedDeskFromAshDeskTemplate(
    const ash::DeskTemplate& desk_template) {
  api::wm_desks_private::Desk out_api_desk;
  out_api_desk.desk_uuid = desk_template.uuid().AsLowercaseString();
  out_api_desk.desk_name = base::UTF16ToUTF8(desk_template.template_name());
  return out_api_desk;
}

}  // namespace

WmDesksPrivateGetDeskTemplateJsonFunction::
    WmDesksPrivateGetDeskTemplateJsonFunction() = default;
WmDesksPrivateGetDeskTemplateJsonFunction::
    ~WmDesksPrivateGetDeskTemplateJsonFunction() = default;

ExtensionFunction::ResponseAction
WmDesksPrivateGetDeskTemplateJsonFunction::Run() {
  std::unique_ptr<api::wm_desks_private::GetDeskTemplateJson::Params> params(
      api::wm_desks_private::GetDeskTemplateJson::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  base::GUID uuid = base::GUID::ParseCaseInsensitive(params->template_uuid);
  if (!uuid.is_valid())
    return RespondNow(Error(kInvalidUuidError));

  DesksClient::Get()->GetTemplateJson(
      uuid, Profile::FromBrowserContext(browser_context()),
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
  DesksClient::Get()->LaunchEmptyDesk(
      base::BindOnce(&WmDesksPrivateLaunchDeskFunction::OnLaunchDesk, this),
      desk_name);
  return AlreadyResponded();
}

void WmDesksPrivateLaunchDeskFunction::OnLaunchDesk(
    std::string error_string,
    const base::GUID& desk_uuid) {
  if (!error_string.empty()) {
    base::UmaHistogramBoolean(kApiLaunchDeskResult, false);
    Respond(Error(std::move(error_string)));
    return;
  }
  base::UmaHistogramBoolean(kApiLaunchDeskResult, true);
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
  return AlreadyResponded();
}

void WmDesksPrivateRemoveDeskFunction::OnRemoveDesk(std::string error_string) {
  if (!error_string.empty()) {
    base::UmaHistogramBoolean(kApiRemoveDeskResult, false);
    Respond(Error(std::move(error_string)));
    return;
  }

  base::UmaHistogramBoolean(kApiRemoveDeskResult, true);
  Respond(NoArguments());
}

WmDesksPrivateGetAllDesksFunction::WmDesksPrivateGetAllDesksFunction() =
    default;
WmDesksPrivateGetAllDesksFunction::~WmDesksPrivateGetAllDesksFunction() =
    default;

ExtensionFunction::ResponseAction WmDesksPrivateGetAllDesksFunction::Run() {
  DesksClient::Get()->GetAllDesks(
      base::BindOnce(&WmDesksPrivateGetAllDesksFunction::OnGetAllDesks, this));
  return AlreadyResponded();
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
  return AlreadyResponded();
}

void WmDesksPrivateSetWindowPropertiesFunction::OnSetWindowProperties(
    std::string error_string) {
  if (!error_string.empty()) {
    base::UmaHistogramBoolean(kApiAllDeskResult, false);
    Respond(Error(std::move(error_string)));
    return;
  }
  base::UmaHistogramBoolean(kApiAllDeskResult, true);
  Respond(NoArguments());
}

WmDesksPrivateSaveActiveDeskFunction::WmDesksPrivateSaveActiveDeskFunction() =
    default;
WmDesksPrivateSaveActiveDeskFunction::~WmDesksPrivateSaveActiveDeskFunction() =
    default;

ExtensionFunction::ResponseAction WmDesksPrivateSaveActiveDeskFunction::Run() {
  DesksClient::Get()->CaptureActiveDeskAndSaveTemplate(
      base::BindOnce(&WmDesksPrivateSaveActiveDeskFunction::OnSavedActiveDesk,
                     this),
      ash::DeskTemplateType::kSaveAndRecall);
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void WmDesksPrivateSaveActiveDeskFunction::OnSavedActiveDesk(
    std::unique_ptr<ash::DeskTemplate> desk_template,
    std::string error_string) {
  if (!error_string.empty()) {
    Respond(Error(std::move(error_string)));
    return;
  }

  // Note that we want to phase out the concept of `template` in external
  // interface. Saved Desk is modeled as desk instead of template in returning
  // value.
  api::wm_desks_private::Desk saved_desk =
      GetSavedDeskFromAshDeskTemplate(*desk_template);
  Respond(ArgumentList(
      api::wm_desks_private::SaveActiveDesk::Results::Create(saved_desk)));
}

WmDesksPrivateDeleteSavedDeskFunction::WmDesksPrivateDeleteSavedDeskFunction() =
    default;
WmDesksPrivateDeleteSavedDeskFunction::
    ~WmDesksPrivateDeleteSavedDeskFunction() = default;

ExtensionFunction::ResponseAction WmDesksPrivateDeleteSavedDeskFunction::Run() {
  std::unique_ptr<api::wm_desks_private::DeleteSavedDesk::Params> params(
      api::wm_desks_private::DeleteSavedDesk::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);
  base::GUID uuid = base::GUID::ParseCaseInsensitive(params->saved_desk_uuid);
  if (!uuid.is_valid())
    return RespondNow(Error(kInvalidUuidError));
  DesksClient::Get()->DeleteDeskTemplate(
      uuid,
      base::BindOnce(&WmDesksPrivateDeleteSavedDeskFunction::OnDeletedSavedDesk,
                     this));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void WmDesksPrivateDeleteSavedDeskFunction::OnDeletedSavedDesk(
    std::string error_string) {
  if (!error_string.empty()) {
    Respond(Error(std::move(error_string)));
    return;
  }
  Respond(NoArguments());
}

WmDesksPrivateRecallSavedDeskFunction::WmDesksPrivateRecallSavedDeskFunction() =
    default;
WmDesksPrivateRecallSavedDeskFunction::
    ~WmDesksPrivateRecallSavedDeskFunction() = default;

ExtensionFunction::ResponseAction WmDesksPrivateRecallSavedDeskFunction::Run() {
  std::unique_ptr<api::wm_desks_private::RecallSavedDesk::Params> params(
      api::wm_desks_private::RecallSavedDesk::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);
  base::GUID uuid = base::GUID::ParseCaseInsensitive(params->saved_desk_uuid);
  if (!uuid.is_valid())
    return RespondNow(Error(kInvalidUuidError));
  DesksClient::Get()->LaunchDeskTemplate(
      uuid,
      base::BindOnce(
          &WmDesksPrivateRecallSavedDeskFunction::OnRecalledSavedDesk, this));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void WmDesksPrivateRecallSavedDeskFunction::OnRecalledSavedDesk(
    std::string error_string,
    const base::GUID& desk_Id) {
  if (!error_string.empty()) {
    Respond(Error(std::move(error_string)));
    return;
  }

  Respond(ArgumentList(api::wm_desks_private::RecallSavedDesk::Results::Create(
      desk_Id.AsLowercaseString())));
}

WmDesksPrivateGetActiveDeskFunction::WmDesksPrivateGetActiveDeskFunction() =
    default;
WmDesksPrivateGetActiveDeskFunction::~WmDesksPrivateGetActiveDeskFunction() =
    default;

ExtensionFunction::ResponseAction WmDesksPrivateGetActiveDeskFunction::Run() {
  base::GUID desk_id = DesksClient::Get()->GetActiveDesk();
  OnGetActiveDesk({}, desk_id);
  return AlreadyResponded();
}

// Error is always empty right now. The interface is to keep compatible
// with future lacros implementation.
void WmDesksPrivateGetActiveDeskFunction::OnGetActiveDesk(
    std::string error_string,
    const base::GUID& desk_Id) {
  if (!error_string.empty()) {
    Respond(Error(std::move(error_string)));
    return;
  }

  Respond(ArgumentList(api::wm_desks_private::GetActiveDesk::Results::Create(
      desk_Id.AsLowercaseString())));
}

WmDesksPrivateSwitchDeskFunction::WmDesksPrivateSwitchDeskFunction() = default;
WmDesksPrivateSwitchDeskFunction::~WmDesksPrivateSwitchDeskFunction() = default;

ExtensionFunction::ResponseAction WmDesksPrivateSwitchDeskFunction::Run() {
  std::unique_ptr<api::wm_desks_private::SwitchDesk::Params> params(
      api::wm_desks_private::SwitchDesk::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);
  base::GUID uuid = base::GUID::ParseCaseInsensitive(params->desk_uuid);
  if (!uuid.is_valid()) {
    base::UmaHistogramBoolean(kApiSwitchDeskResult, false);
    return RespondNow(Error(kInvalidDeskIdError));
  }
  std::string error = DesksClient::Get()->SwitchDesk(uuid);
  OnSwitchDesk(error);
  return AlreadyResponded();
}

// The interface is to keep compatible with future lacros implementation.
void WmDesksPrivateSwitchDeskFunction::OnSwitchDesk(std::string error_string) {
  if (!error_string.empty()) {
    base::UmaHistogramBoolean(kApiSwitchDeskResult, false);
    Respond(Error(std::move(error_string)));
    return;
  }
  base::UmaHistogramBoolean(kApiSwitchDeskResult, true);
  Respond(NoArguments());
}

}  // namespace extensions
