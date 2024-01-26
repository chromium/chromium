// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/wm/wm_desks_private_api.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_functions.h"
#include "base/uuid.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/wm_desks_private.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/chromeos/extensions/wm/wm_desks_private_feature_lacros.h"
#else
#include "chrome/browser/chromeos/extensions/wm/wm_desks_private_feature_ash.h"
#endif

namespace extensions {

namespace {

constexpr char kApiLaunchDeskResult[] = "Ash.DeskApi.LaunchDesk.Result";
constexpr char kApiRemoveDeskResult[] = "Ash.DeskApi.RemoveDesk.Result";
constexpr char kApiSwitchDeskResult[] = "Ash.DeskApi.SwitchDesk.Result";
constexpr char kApiAllDeskResult[] = "Ash.DeskApi.AllDesk.Result";
constexpr char kInvalidIdError[] = "InvalidIdError";
constexpr char kStorageError[] = "StorageError";

std::unique_ptr<WMDesksPrivateFeature> GetDeskFeatureImpl() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return std::make_unique<WMDesksPrivateFeatureLacros>();
#else
  return std::make_unique<WMDesksPrivateFeatureAsh>();
#endif
}

}  // namespace

WmDesksPrivateGetSavedDesksFunction::WmDesksPrivateGetSavedDesksFunction() =
    default;
WmDesksPrivateGetSavedDesksFunction::~WmDesksPrivateGetSavedDesksFunction() =
    default;

ExtensionFunction::ResponseAction WmDesksPrivateGetSavedDesksFunction::Run() {
  std::unique_ptr<WMDesksPrivateFeature> desk_impl = GetDeskFeatureImpl();
  desk_impl->GetSavedDesks(base::BindOnce(
      &WmDesksPrivateGetSavedDesksFunction::OnGetSavedDesks, this));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void WmDesksPrivateGetSavedDesksFunction::OnGetSavedDesks(
    std::string error_string,
    std::vector<api::wm_desks_private::SavedDesk> desks) {
  if (!error_string.empty()) {
    Respond(Error(std::move(error_string)));
    return;
  }

  Respond(ArgumentList(
      api::wm_desks_private::GetSavedDesks::Results::Create(std::move(desks))));
}

WmDesksPrivateGetDeskTemplateJsonFunction::
    WmDesksPrivateGetDeskTemplateJsonFunction() = default;
WmDesksPrivateGetDeskTemplateJsonFunction::
    ~WmDesksPrivateGetDeskTemplateJsonFunction() = default;

ExtensionFunction::ResponseAction
WmDesksPrivateGetDeskTemplateJsonFunction::Run() {
  std::optional<api::wm_desks_private::GetDeskTemplateJson::Params> params =
      api::wm_desks_private::GetDeskTemplateJson::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  base::Uuid uuid = base::Uuid::ParseCaseInsensitive(params->template_uuid);
  if (!uuid.is_valid()) {
    return RespondNow(Error(kInvalidIdError));
  }

  std::unique_ptr<WMDesksPrivateFeature> desk_impl = GetDeskFeatureImpl();
  desk_impl->GetDeskTemplateJson(
      uuid, Profile::FromBrowserContext(browser_context()),
      base::BindOnce(
          &WmDesksPrivateGetDeskTemplateJsonFunction::OnGetDeskTemplateJson,
          this));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void WmDesksPrivateGetDeskTemplateJsonFunction::OnGetDeskTemplateJson(
    std::string error,
    base::Value template_json) {
  if (!error.empty()) {
    Respond(Error(std::move(error)));
    return;
  }
  std::string raw_json_string;
  const bool conversion_success =
      base::JSONWriter::Write(template_json, &raw_json_string);

  if (!conversion_success) {
    Respond(Error(std::move(kStorageError)));
    return;
  }
  Respond(
      ArgumentList(api::wm_desks_private::GetDeskTemplateJson::Results::Create(
          raw_json_string)));
}

WmDesksPrivateLaunchDeskFunction::WmDesksPrivateLaunchDeskFunction() = default;
WmDesksPrivateLaunchDeskFunction::~WmDesksPrivateLaunchDeskFunction() = default;

ExtensionFunction::ResponseAction WmDesksPrivateLaunchDeskFunction::Run() {
  std::optional<api::wm_desks_private::LaunchDesk::Params> params =
      api::wm_desks_private::LaunchDesk::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  auto& launch_options = params->launch_options;
  std::string desk_name = launch_options.desk_name.value_or("");
  std::unique_ptr<WMDesksPrivateFeature> desk_impl = GetDeskFeatureImpl();
  desk_impl->LaunchDesk(
      desk_name,
      base::BindOnce(&WmDesksPrivateLaunchDeskFunction::OnLaunchDesk, this));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void WmDesksPrivateLaunchDeskFunction::OnLaunchDesk(
    std::string error,
    const base::Uuid& desk_uuid) {
  if (!error.empty()) {
    base::UmaHistogramBoolean(kApiLaunchDeskResult, false);
    Respond(Error(std::move(error)));
    return;
  }
  base::UmaHistogramBoolean(kApiLaunchDeskResult, true);
  Respond(ArgumentList(api::wm_desks_private::LaunchDesk::Results::Create(
      desk_uuid.AsLowercaseString())));
}

WmDesksPrivateRemoveDeskFunction::WmDesksPrivateRemoveDeskFunction() = default;
WmDesksPrivateRemoveDeskFunction::~WmDesksPrivateRemoveDeskFunction() = default;

ExtensionFunction::ResponseAction WmDesksPrivateRemoveDeskFunction::Run() {
  std::optional<api::wm_desks_private::RemoveDesk::Params> params =
      api::wm_desks_private::RemoveDesk::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  bool combine_desk = params->remove_desk_options
                          ? params->remove_desk_options->combine_desks
                          : false;
  bool allow_undo =
      params->remove_desk_options
          ? params->remove_desk_options->allow_undo.value_or(false)
          : false;
  base::Uuid uuid = base::Uuid::ParseCaseInsensitive(params->desk_id);
  if (!uuid.is_valid()) {
    base::UmaHistogramBoolean(kApiRemoveDeskResult, false);
    return RespondNow(Error(kInvalidIdError));
  }
  std::unique_ptr<WMDesksPrivateFeature> desk_impl = GetDeskFeatureImpl();
  desk_impl->RemoveDesk(
      uuid, combine_desk, allow_undo,
      base::BindOnce(&WmDesksPrivateRemoveDeskFunction::OnRemoveDesk, this));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void WmDesksPrivateRemoveDeskFunction::OnRemoveDesk(std::string error) {
  if (!error.empty()) {
    base::UmaHistogramBoolean(kApiRemoveDeskResult, false);

    Respond(Error(std::move(error)));
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
  std::unique_ptr<WMDesksPrivateFeature> desk_impl = GetDeskFeatureImpl();
  desk_impl->GetAllDesks(
      base::BindOnce(&WmDesksPrivateGetAllDesksFunction::OnGetAllDesks, this));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void WmDesksPrivateGetAllDesksFunction::OnGetAllDesks(
    std::string error,
    std::vector<api::wm_desks_private::Desk> desks) {
  if (!error.empty()) {
    Respond(Error(std::move(error)));
    return;
  }
  Respond(
      ArgumentList(api::wm_desks_private::GetAllDesks::Results::Create(desks)));
}

WmDesksPrivateSetWindowPropertiesFunction::
    WmDesksPrivateSetWindowPropertiesFunction() = default;
WmDesksPrivateSetWindowPropertiesFunction::
    ~WmDesksPrivateSetWindowPropertiesFunction() = default;

ExtensionFunction::ResponseAction
WmDesksPrivateSetWindowPropertiesFunction::Run() {
  std::optional<api::wm_desks_private::SetWindowProperties::Params> params =
      api::wm_desks_private::SetWindowProperties::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  std::unique_ptr<WMDesksPrivateFeature> desk_impl = GetDeskFeatureImpl();
  desk_impl->SetAllDeskProperty(
      params->window_id, params->window_properties.all_desks,
      base::BindOnce(
          &WmDesksPrivateSetWindowPropertiesFunction::OnSetWindowProperties,
          this));
  return did_respond() ? AlreadyResponded() : RespondLater();
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
  std::unique_ptr<WMDesksPrivateFeature> desk_impl = GetDeskFeatureImpl();
  desk_impl->SaveActiveDesk(base::BindOnce(
      &WmDesksPrivateSaveActiveDeskFunction::OnSavedActiveDesk, this));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void WmDesksPrivateSaveActiveDeskFunction::OnSavedActiveDesk(
    std::string error,
    api::wm_desks_private::SavedDesk saved_desk) {
  if (!error.empty()) {
    Respond(Error(std::move(error)));
    return;
  }
  Respond(ArgumentList(
      api::wm_desks_private::SaveActiveDesk::Results::Create(saved_desk)));
}

WmDesksPrivateDeleteSavedDeskFunction::WmDesksPrivateDeleteSavedDeskFunction() =
    default;
WmDesksPrivateDeleteSavedDeskFunction::
    ~WmDesksPrivateDeleteSavedDeskFunction() = default;

ExtensionFunction::ResponseAction WmDesksPrivateDeleteSavedDeskFunction::Run() {
  std::optional<api::wm_desks_private::DeleteSavedDesk::Params> params =
      api::wm_desks_private::DeleteSavedDesk::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  base::Uuid uuid = base::Uuid::ParseCaseInsensitive(params->saved_desk_uuid);
  if (!uuid.is_valid()) {
    return RespondNow(Error(kInvalidIdError));
  }
  std::unique_ptr<WMDesksPrivateFeature> desk_impl = GetDeskFeatureImpl();

  desk_impl->DeleteSavedDesk(
      uuid,
      base::BindOnce(&WmDesksPrivateDeleteSavedDeskFunction::OnDeletedSavedDesk,
                     this));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void WmDesksPrivateDeleteSavedDeskFunction::OnDeletedSavedDesk(
    std::string error) {
  if (!error.empty()) {
    Respond(Error(std::move(error)));
    return;
  }

  Respond(NoArguments());
}

WmDesksPrivateRecallSavedDeskFunction::WmDesksPrivateRecallSavedDeskFunction() =
    default;
WmDesksPrivateRecallSavedDeskFunction::
    ~WmDesksPrivateRecallSavedDeskFunction() = default;

ExtensionFunction::ResponseAction WmDesksPrivateRecallSavedDeskFunction::Run() {
  std::optional<api::wm_desks_private::RecallSavedDesk::Params> params =
      api::wm_desks_private::RecallSavedDesk::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  base::Uuid uuid = base::Uuid::ParseCaseInsensitive(params->saved_desk_uuid);
  if (!uuid.is_valid()) {
    return RespondNow(Error(kInvalidIdError));
  }
  std::unique_ptr<WMDesksPrivateFeature> desk_impl = GetDeskFeatureImpl();
  desk_impl->RecallSavedDesk(
      uuid,
      base::BindOnce(
          &WmDesksPrivateRecallSavedDeskFunction::OnRecalledSavedDesk, this));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void WmDesksPrivateRecallSavedDeskFunction::OnRecalledSavedDesk(
    std::string error,
    const base::Uuid& desk_Id) {
  if (!error.empty()) {
    Respond(Error(std::move(error)));
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
  std::unique_ptr<WMDesksPrivateFeature> desk_impl = GetDeskFeatureImpl();
  desk_impl->GetActiveDesk(base::BindOnce(
      &WmDesksPrivateGetActiveDeskFunction::OnGetActiveDesk, this));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void WmDesksPrivateGetActiveDeskFunction::OnGetActiveDesk(
    std::string error_string,
    const base::Uuid& desk_Id) {
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
  std::optional<api::wm_desks_private::SwitchDesk::Params> params =
      api::wm_desks_private::SwitchDesk::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  base::Uuid uuid = base::Uuid::ParseCaseInsensitive(params->desk_uuid);
  if (!uuid.is_valid()) {
    base::UmaHistogramBoolean(kApiSwitchDeskResult, false);
    return RespondNow(Error(kInvalidIdError));
  }
  std::unique_ptr<WMDesksPrivateFeature> desk_impl = GetDeskFeatureImpl();
  desk_impl->SwitchDesk(
      uuid,
      base::BindOnce(&WmDesksPrivateSwitchDeskFunction::OnSwitchDesk, this));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void WmDesksPrivateSwitchDeskFunction::OnSwitchDesk(std::string error_string) {
  if (!error_string.empty()) {
    base::UmaHistogramBoolean(kApiSwitchDeskResult, false);
    Respond(Error(std::move(error_string)));
    return;
  }
  base::UmaHistogramBoolean(kApiSwitchDeskResult, true);
  Respond(NoArguments());
}

WmDesksPrivateGetDeskByIDFunction::WmDesksPrivateGetDeskByIDFunction() =
    default;
WmDesksPrivateGetDeskByIDFunction::~WmDesksPrivateGetDeskByIDFunction() =
    default;

ExtensionFunction::ResponseAction WmDesksPrivateGetDeskByIDFunction::Run() {
  auto params = api::wm_desks_private::GetDeskByID::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  base::Uuid uuid = base::Uuid::ParseCaseInsensitive(params->desk_uuid);
  if (!uuid.is_valid()) {
    return RespondNow(Error(kInvalidIdError));
  }
  std::unique_ptr<WMDesksPrivateFeature> desk_impl = GetDeskFeatureImpl();
  desk_impl->GetDeskByID(
      uuid,
      base::BindOnce(&WmDesksPrivateGetDeskByIDFunction::OnGetDeskByID, this));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void WmDesksPrivateGetDeskByIDFunction::OnGetDeskByID(
    std::string error_string,
    api::wm_desks_private::Desk desk) {
  if (!error_string.empty()) {
    Respond(Error(std::move(error_string)));
    return;
  }

  Respond(
      ArgumentList(api::wm_desks_private::GetDeskByID::Results::Create(desk)));
}

}  // namespace extensions
