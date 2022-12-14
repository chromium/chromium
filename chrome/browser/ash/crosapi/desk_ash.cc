// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/desk_ash.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/desk_template.h"
#include "ash/shell.h"
#include "ash/wm/desks/desk.h"
#include "base/guid.h"
#include "base/strings/utf_string_conversions.h"
#include "base/value_iterators.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/desks/desks_client.h"
#include "chromeos/crosapi/mojom/desk.mojom-forward.h"
#include "chromeos/crosapi/mojom/desk.mojom-shared.h"
#include "components/app_restore/window_properties.h"
#include "ui/aura/client/aura_constants.h"

namespace crosapi {

namespace {

crosapi::mojom::DeskModelPtr ToDeskModel(const ash::Desk* desk) {
  auto desk_model = crosapi::mojom::DeskModel::New();
  desk_model->desk_uuid = desk->uuid().AsLowercaseString();
  desk_model->desk_name = base::UTF16ToUTF8(desk->name());
  return desk_model;
}

crosapi::mojom::SavedDeskType ToSavedDeskType(
    const ash::DeskTemplateType type) {
  switch (type) {
    case ash::DeskTemplateType::kTemplate:
      return crosapi::mojom::SavedDeskType::kTemplate;
    case ash::DeskTemplateType::kSaveAndRecall:
      return crosapi::mojom::SavedDeskType::kSaveAndRecall;
    // Desk API does not save/restore Floating Workspace.
    case ash::DeskTemplateType::kFloatingWorkspace:
    case ash::DeskTemplateType::kUnknown:
      return crosapi::mojom::SavedDeskType::kUnknown;
  }
}

crosapi::mojom::SavedDeskModelPtr ToSavedDeskModel(
    const ash::DeskTemplate* saved_desk) {
  auto saved_desk_model = crosapi::mojom::SavedDeskModel::New();
  saved_desk_model->saved_desk_uuid = saved_desk->uuid().AsLowercaseString();
  saved_desk_model->saved_desk_name =
      base::UTF16ToUTF8(saved_desk->template_name());
  saved_desk_model->saved_desk_type = ToSavedDeskType(saved_desk->type());
  return saved_desk_model;
}

crosapi::mojom::DeskCrosApiError ToCrosApiError(
    const DesksClient::DeskActionError result) {
  switch (result) {
    case DesksClient::DeskActionError::kStorageError:
      return crosapi::mojom::DeskCrosApiError::kStorageError;
    case DesksClient::DeskActionError::kNoCurrentUserError:
      return crosapi::mojom::DeskCrosApiError::kNoCurrentUserError;
    case DesksClient::DeskActionError::kBadProfileError:
      return crosapi::mojom::DeskCrosApiError::kBadProfileError;
    case DesksClient::DeskActionError::kResourceNotFoundError:
      return crosapi::mojom::DeskCrosApiError::kResourceNotFoundError;
    case DesksClient::DeskActionError::kInvalidIdError:
      return crosapi::mojom::DeskCrosApiError::kInvalidIdError;
    case DesksClient::DeskActionError::kDesksBeingModifiedError:
      return crosapi::mojom::DeskCrosApiError::kDesksBeingModifiedError;
    case DesksClient::DeskActionError::kDesksCountCheckFailedError:
      return crosapi::mojom::DeskCrosApiError::kDesksCountCheckFailedError;
    case DesksClient::DeskActionError::kUnknownError:
      return crosapi::mojom::DeskCrosApiError::kUnknownError;
  }
}

}  // namespace

DeskAsh::DeskAsh() = default;

DeskAsh::~DeskAsh() = default;

void DeskAsh::BindReceiver(
    mojo::PendingReceiver<mojom::Desk> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void DeskAsh::LaunchEmptyDesk(const std::string& desk_name,
                              LaunchEmptyDeskCallback callback) {
  DesksClient::Get()->LaunchEmptyDesk(
      base::BindOnce(
          [](LaunchEmptyDeskCallback callback,
             absl::optional<DesksClient::DeskActionError> error,
             const base::GUID& desk_uuid) {
            if (error) {
              std::move(callback).Run(
                  crosapi::mojom::LaunchEmptyDeskResult::NewError(
                      ToCrosApiError(error.value())));
              return;
            }
            auto result =
                crosapi::mojom::LaunchEmptyDeskResult::NewDeskId(desk_uuid);
            std::move(callback).Run(std::move(result));
          },
          std::move(callback)),
      base::UTF8ToUTF16(desk_name));
}

void DeskAsh::RemoveDesk(const base::GUID& desk_uuid,
                         bool close_all,
                         RemoveDeskCallback callback) {
  DesksClient::Get()->RemoveDesk(
      desk_uuid, close_all,
      base::BindOnce(
          [](RemoveDeskCallback callback,
             absl::optional<DesksClient::DeskActionError> error) {
            if (error) {
              std::move(callback).Run(
                  crosapi::mojom::RemoveDeskResult::NewError(
                      ToCrosApiError(error.value())));
              return;
            }
            auto result = crosapi::mojom::RemoveDeskResult::NewSucceeded(true);
            std::move(callback).Run(std::move(result));
          },
          std::move(callback)));
}

void DeskAsh::GetTemplateJson(const base::GUID& uuid,
                              GetTemplateJsonCallback callback) {
  DesksClient::Get()->GetTemplateJson(
      uuid, ProfileManager::GetActiveUserProfile(),
      base::BindOnce(
          [](GetTemplateJsonCallback callback,
             absl::optional<DesksClient::DeskActionError> error,
             const base::Value& template_json) {
            if (error) {
              std::move(callback).Run(
                  crosapi::mojom::GetTemplateJsonResult::NewError(
                      ToCrosApiError(error.value())));
              return;
            }
            auto result =
                crosapi::mojom::GetTemplateJsonResult::NewTemplateJson(
                    template_json.Clone());
            std::move(callback).Run(std::move(result));
          },
          std::move(callback)));
}

void DeskAsh::GetAllDesks(GetAllDesksCallback callback) {
  DesksClient::Get()->GetAllDesks(base::BindOnce(
      [](GetAllDesksCallback callback,
         absl::optional<DesksClient::DeskActionError> error,
         const std::vector<const ash::Desk*>& desks) {
        if (error) {
          std::move(callback).Run(crosapi::mojom::GetAllDesksResult::NewError(
              ToCrosApiError(error.value())));
          return;
        }
        std::vector<crosapi::mojom::DeskModelPtr> cros_desks;
        for (const auto* d : desks) {
          cros_desks.push_back(ToDeskModel(d));
        }
        auto result =
            crosapi::mojom::GetAllDesksResult::NewDesks(std::move(cros_desks));
        std::move(callback).Run(std::move(result));
      },
      std::move(callback)));
}

void DeskAsh::SaveActiveDesk(SaveActiveDeskCallback callback) {
  DesksClient::Get()->CaptureActiveDeskAndSaveTemplate(
      base::BindOnce(
          [](SaveActiveDeskCallback callback,
             absl::optional<DesksClient::DeskActionError> error,
             std::unique_ptr<ash::DeskTemplate> desk_template) {
            if (error) {
              std::move(callback).Run(
                  crosapi::mojom::SaveActiveDeskResult::NewError(
                      ToCrosApiError(error.value())));
              return;
            }
            crosapi::mojom::DeskModelPtr desk_model(
                crosapi::mojom::DeskModel::New());
            desk_model->desk_uuid = desk_template->uuid().AsLowercaseString();
            desk_model->desk_name =
                base::UTF16ToUTF8(desk_template->template_name());
            auto result = crosapi::mojom::SaveActiveDeskResult::NewSavedDesk(
                std::move(desk_model));

            std::move(callback).Run(std::move(result));
          },
          std::move(callback)),
      ash::DeskTemplateType::kSaveAndRecall);
}

void DeskAsh::DeleteSavedDesk(const base::GUID& uuid,
                              DeleteSavedDeskCallback callback) {
  DesksClient::Get()->DeleteDeskTemplate(
      uuid, base::BindOnce(
                [](DeleteSavedDeskCallback callback,
                   absl::optional<DesksClient::DeskActionError> error) {
                  if (error) {
                    std::move(callback).Run(
                        crosapi::mojom::DeleteSavedDeskResult::NewError(
                            ToCrosApiError(error.value())));
                    return;
                  }
                  auto result =
                      crosapi::mojom::DeleteSavedDeskResult::NewSucceeded(true);
                  std::move(callback).Run(std::move(result));
                },
                std::move(callback)));
}

void DeskAsh::RecallSavedDesk(const base::GUID& uuid,
                              RecallSavedDeskCallback callback) {
  DesksClient::Get()->LaunchDeskTemplate(
      uuid,
      base::BindOnce(
          [](RecallSavedDeskCallback callback,
             absl::optional<DesksClient::DeskActionError> error,
             const base::GUID& desk_uuid) {
            if (error) {
              std::move(callback).Run(
                  crosapi::mojom::RecallSavedDeskResult::NewError(
                      ToCrosApiError(error.value())));
              return;
            }
            auto result =
                crosapi::mojom::RecallSavedDeskResult::NewDeskId(desk_uuid);
            std::move(callback).Run(std::move(result));
          },
          std::move(callback)));
}

// Note: This solution is coupled with app restore project. We need an
// identifier that's consistent between lacros-chrome and ash-chrome.
// We're reusing the `app_restore_window_id` here for the unique identifier for
// ash windows, which essentially is the `browser_session_id` for browsers.
void DeskAsh::SetAllDesksProperty(int32_t app_restore_window_id,
                                  bool all_desk,
                                  SetAllDesksPropertyCallback callback) {
  for (aura::Window* root : ash::Shell::GetAllRootWindows()) {
    aura::Window* target =
        GetWindowByAppRestoreWindowId(root, app_restore_window_id);
    if (target) {
      target->SetProperty(
          aura::client::kWindowWorkspaceKey,
          all_desk ? aura::client::kWindowWorkspaceVisibleOnAllWorkspaces
                   : aura::client::kWindowWorkspaceUnassignedWorkspace);
      std::move(callback).Run(
          crosapi::mojom::SetAllDesksPropertyResult::NewSucceeded(true));
      return;
    }
  }
  std::move(callback).Run(crosapi::mojom::SetAllDesksPropertyResult::NewError(
      mojom::DeskCrosApiError::kResourceNotFoundError));
}

void DeskAsh::GetSavedDesks(GetSavedDesksCallback callback) {
  DesksClient::Get()->GetDeskTemplates(base::BindOnce(
      [](GetSavedDesksCallback callback,
         absl::optional<DesksClient::DeskActionError> error,
         const std::vector<const ash::DeskTemplate*>& desk_templates) {
        if (error) {
          std::move(callback).Run(crosapi::mojom::GetSavedDesksResult::NewError(
              ToCrosApiError(error.value())));
          return;
        }
        std::vector<crosapi::mojom::SavedDeskModelPtr> saved_desks;
        for (auto* desk_template : desk_templates) {
          crosapi::mojom::SavedDeskModelPtr saved_desk =
              ToSavedDeskModel(desk_template);
          saved_desks.push_back(std::move(saved_desk));
        }
        std::move(callback).Run(
            crosapi::mojom::GetSavedDesksResult::NewSavedDesks(
                std::move(saved_desks)));
      },
      std::move(callback)));
}

void DeskAsh::GetActiveDesk(GetActiveDeskCallback callback) {
  auto desk_id = DesksClient::Get()->GetActiveDesk();
  std::move(callback).Run(
      crosapi::mojom::GetActiveDeskResult::NewDeskId(desk_id));
}

void DeskAsh::SwitchDesk(const base::GUID& desk_id,
                         SwitchDeskCallback callback) {
  auto error = DesksClient::Get()->SwitchDesk(desk_id);
  if (error) {
    std::move(callback).Run(crosapi::mojom::SwitchDeskResult::NewError(
        ToCrosApiError(error.value())));
    return;
  }
  std::move(callback).Run(crosapi::mojom::SwitchDeskResult::NewSucceeded(true));
}

// Performs a depth-first search for a window with given App Restore Window Id.
aura::Window* DeskAsh::GetWindowByAppRestoreWindowId(
    aura::Window* window,
    int32_t app_restore_window_id) {
  if (window->GetProperty(app_restore::kWindowIdKey) == app_restore_window_id)
    return window;
  for (aura::Window* child : window->children()) {
    aura::Window* target =
        GetWindowByAppRestoreWindowId(child, app_restore_window_id);
    if (target)
      return target;
  }
  return nullptr;
}

}  // namespace crosapi
