// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/public/cpp/desk_template.h"
#include "ash/wm/desks/desk.h"
#include "base/functional/bind.h"
#include "base/guid.h"
#include "base/strings/utf_string_conversions.h"
#include "base/value_iterators.h"
#include "chrome/browser/chromeos/extensions/wm/wm_desks_private_feature_ash.h"
#include "chrome/browser/ui/ash/desks/desks_client.h"

namespace extensions {

namespace {
std::string GetStringError(DesksClient::DeskActionError error) {
  switch (error) {
    case DesksClient::DeskActionError::kStorageError:
      return "StorageError";
    case DesksClient::DeskActionError::kNoCurrentUserError:
      return "NoCurrentUserError";
    case DesksClient::DeskActionError::kBadProfileError:
      return "BadProfileError";
    case DesksClient::DeskActionError::kResourceNotFoundError:
      return "ResourceNotFoundError";
    case DesksClient::DeskActionError::kInvalidIdError:
      return "InvalidIdError";
    case DesksClient::DeskActionError::kDesksBeingModifiedError:
      return "DeskBeingModifiedError";
    case DesksClient::DeskActionError::kDesksCountCheckFailedError:
      return "DesksCountCheckFailedError";
    case DesksClient::DeskActionError::kUnknownError:
      return "UnknownError";
  }
}

api::wm_desks_private::SavedDeskType GetSavedDeskTypeFromDeskTemplateType(
    const ash::DeskTemplateType type) {
  switch (type) {
    case ash::DeskTemplateType::kTemplate:
      return api::wm_desks_private::SavedDeskType::SAVED_DESK_TYPE_KTEMPLATE;
    case ash::DeskTemplateType::kSaveAndRecall:
      return api::wm_desks_private::SavedDeskType::
          SAVED_DESK_TYPE_KSAVEANDRECALL;
    case ash::DeskTemplateType::kFloatingWorkspace:
      // Desk API does not save/restore Floating Workspace.
      return api::wm_desks_private::SavedDeskType::SAVED_DESK_TYPE_KUNKNOWN;
    case ash::DeskTemplateType::kUnknown:
      return api::wm_desks_private::SavedDeskType::SAVED_DESK_TYPE_KUNKNOWN;
  }
}

api::wm_desks_private::Desk GetDeskFromAshDesk(const ash::Desk& ash_desk) {
  api::wm_desks_private::Desk target;
  target.desk_name = base::UTF16ToUTF8(ash_desk.name());
  target.desk_uuid = ash_desk.uuid().AsLowercaseString();
  return target;
}

api::wm_desks_private::SavedDesk GetSavedDeskFromAshDeskTemplate(
    const ash::DeskTemplate& desk_template) {
  api::wm_desks_private::SavedDesk out_api_desk;
  out_api_desk.saved_desk_uuid = desk_template.uuid().AsLowercaseString();
  out_api_desk.saved_desk_name =
      base::UTF16ToUTF8(desk_template.template_name());
  out_api_desk.saved_desk_type =
      GetSavedDeskTypeFromDeskTemplateType(desk_template.type());
  return out_api_desk;
}

}  // namespace

WMDesksPrivateFeatureAsh::WMDesksPrivateFeatureAsh() = default;

WMDesksPrivateFeatureAsh::~WMDesksPrivateFeatureAsh() = default;

void WMDesksPrivateFeatureAsh::GetDeskTemplateJson(
    const base::GUID& template_uuid,
    Profile* profile,
    GetDeskTemplateJsonCallback callback) {
  DesksClient::Get()->GetTemplateJson(
      template_uuid, profile,
      base::BindOnce(
          [](GetDeskTemplateJsonCallback callback,
             absl::optional<DesksClient::DeskActionError> error,
             const base::Value& template_json) {
            if (error) {
              std::move(callback).Run(GetStringError(error.value()), {});
            } else {
              std::move(callback).Run({}, template_json.Clone());
            }
          },
          std::move(callback)));
}

void WMDesksPrivateFeatureAsh::LaunchDesk(std::string desk_name,
                                          LaunchDeskCallback callback) {
  auto result =
      DesksClient::Get()->LaunchEmptyDesk(base::UTF8ToUTF16(desk_name));
  if (!result.has_value()) {
    std::move(callback).Run(GetStringError(result.error()), {});
    return;
  }
  std::move(callback).Run({}, result.value());
}

void WMDesksPrivateFeatureAsh::RemoveDesk(const base::GUID& desk_uuid,
                                          bool combine_desk,
                                          RemoveDeskCallback callback) {
  auto error = DesksClient::Get()->RemoveDesk(desk_uuid, combine_desk);
  std::move(callback).Run(error ? GetStringError(error.value()) : "");
}

void WMDesksPrivateFeatureAsh::SetAllDeskProperty(
    int32_t window_id,
    bool all_desks,
    SetAllDeskPropertyCallback callback) {
  auto error = DesksClient::Get()->SetAllDeskPropertyByBrowserSessionId(
      SessionID::FromSerializedValue(window_id), all_desks);
  std::move(callback).Run(error ? GetStringError(error.value()) : "");
}

void WMDesksPrivateFeatureAsh::GetAllDesks(GetAllDesksCallback callback) {
  auto result = DesksClient::Get()->GetAllDesks();
  if (!result.has_value()) {
    std::move(callback).Run(GetStringError(result.error()), {});
    return;
  }
  std::vector<api::wm_desks_private::Desk> api_desks;
  for (const ash::Desk* desk : result.value()) {
    api_desks.push_back(GetDeskFromAshDesk(*desk));
  }
  std::move(callback).Run({}, std::move(api_desks));
}

void WMDesksPrivateFeatureAsh::SaveActiveDesk(SaveActiveDeskCallback callback) {
  DesksClient::Get()->CaptureActiveDeskAndSaveTemplate(
      base::BindOnce(
          [](SaveActiveDeskCallback callback,
             absl::optional<DesksClient::DeskActionError> error,
             std::unique_ptr<ash::DeskTemplate> desk_template) {
            // Note that we want to phase out the concept of `template` in
            // external interface. Use `saved_desk` model instead of template.
            if (error) {
              std::move(callback).Run(GetStringError(error.value()), {});
            } else {
              api::wm_desks_private::SavedDesk saved_desk =
                  GetSavedDeskFromAshDeskTemplate(*desk_template);
              std::move(callback).Run({}, std::move(saved_desk));
            }
          },
          std::move(callback)),
      ash::DeskTemplateType::kSaveAndRecall);
}

void WMDesksPrivateFeatureAsh::DeleteSavedDesk(
    const base::GUID& desk_uuid,
    DeleteSavedDeskCallback callback) {
  DesksClient::Get()->DeleteDeskTemplate(
      desk_uuid,
      base::BindOnce(
          [](DeleteSavedDeskCallback callback,
             absl::optional<DesksClient::DeskActionError> error) {
            std::move(callback).Run(error ? GetStringError(error.value()) : "");
          },
          std::move(callback)));
}

void WMDesksPrivateFeatureAsh::RecallSavedDesk(
    const base::GUID& desk_uuid,
    RecallSavedDeskCallback callback) {
  DesksClient::Get()->LaunchDeskTemplate(
      desk_uuid, base::BindOnce(
                     [](RecallSavedDeskCallback callback,
                        absl::optional<DesksClient::DeskActionError> error,
                        const base::GUID& desk_Id) {
                       if (error) {
                         std::move(callback).Run(GetStringError(error.value()),
                                                 {});
                       } else {
                         std::move(callback).Run({}, std::move(desk_Id));
                       }
                     },
                     std::move(callback)));
}

void WMDesksPrivateFeatureAsh::GetSavedDesks(GetSavedDesksCallback callback) {
  DesksClient::Get()->GetDeskTemplates(base::BindOnce(
      [](GetSavedDesksCallback callback,
         absl::optional<DesksClient::DeskActionError> error,
         const std::vector<const ash::DeskTemplate*>& desk_templates) {
        if (error) {
          std::move(callback).Run(GetStringError(error.value()), {});
        } else {
          std::vector<api::wm_desks_private::SavedDesk> api_templates;
          for (auto* desk_template : desk_templates) {
            api::wm_desks_private::SavedDesk saved_desk =
                GetSavedDeskFromAshDeskTemplate(*desk_template);
            api_templates.push_back(std::move(saved_desk));
          }
          std::move(callback).Run({}, std::move(api_templates));
        }
      },
      std::move(callback)));
}

void WMDesksPrivateFeatureAsh::GetActiveDesk(GetActiveDeskCallback callback) {
  base::GUID desk_id = DesksClient::Get()->GetActiveDesk();
  std::move(callback).Run({}, desk_id);
}

void WMDesksPrivateFeatureAsh::SwitchDesk(const base::GUID& desk_uuid,
                                          SwitchDeskCallback callback) {
  auto error = DesksClient::Get()->SwitchDesk(desk_uuid);
  std::move(callback).Run(error ? GetStringError(error.value()) : "");
}

}  // namespace extensions
