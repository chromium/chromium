// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/guid.h"
#include "chrome/browser/chromeos/extensions/wm/wm_desks_private_api.h"
#include "chrome/browser/chromeos/extensions/wm/wm_desks_private_feature_lacros.h"
#include "chrome/common/extensions/api/wm_desks_private.h"
#include "chromeos/crosapi/mojom/desk.mojom-forward.h"
#include "chromeos/crosapi/mojom/desk.mojom-shared.h"
#include "chromeos/crosapi/mojom/desk.mojom.h"
#include "chromeos/lacros/lacros_service.h"

namespace extensions {

namespace {

constexpr char kCROS_API_UNAVAILABLE[] = "CrosAPIUnavailableError";

api::wm_desks_private::Desk GetDeskFromCrosApiDesk(
    const crosapi::mojom::DeskModelPtr& cros_api_desk) {
  api::wm_desks_private::Desk target;
  target.desk_name = cros_api_desk->desk_name;
  target.desk_uuid = cros_api_desk->desk_uuid;
  return target;
}

api::wm_desks_private::SavedDesk GetSavedDeskFromCrosApiDesk(
    const crosapi::mojom::DeskModelPtr& cros_api_desk) {
  api::wm_desks_private::SavedDesk target;
  target.saved_desk_name = cros_api_desk->desk_name;
  target.saved_desk_uuid = cros_api_desk->desk_uuid;
  return target;
}

api::wm_desks_private::SavedDeskType ToSavedDeskType(
    const crosapi::mojom::SavedDeskType type) {
  switch (type) {
    case crosapi::mojom::SavedDeskType::kTemplate:
      return api::wm_desks_private::SavedDeskType::SAVED_DESK_TYPE_KTEMPLATE;
    case crosapi::mojom::SavedDeskType::kSaveAndRecall:
      return api::wm_desks_private::SavedDeskType::
          SAVED_DESK_TYPE_KSAVEANDRECALL;
    case crosapi::mojom::SavedDeskType::kUnknown:
      return api::wm_desks_private::SavedDeskType::SAVED_DESK_TYPE_KUNKNOWN;
  }
}

api::wm_desks_private::SavedDesk GetSavedDeskFromCrosApiSavedDesk(
    const crosapi::mojom::SavedDeskModelPtr& cros_api_saved_desk) {
  api::wm_desks_private::SavedDesk target;
  target.saved_desk_name = cros_api_saved_desk->saved_desk_name;
  target.saved_desk_uuid = cros_api_saved_desk->saved_desk_uuid;
  target.saved_desk_type =
      ToSavedDeskType(cros_api_saved_desk->saved_desk_type);
  return target;
}

std::string GetStringError(crosapi::mojom::DeskCrosApiError result) {
  switch (result) {
    case crosapi::mojom::DeskCrosApiError::kStorageError:
    // TODO(aprilzhou): Deprecate this enum and map it to kUnknownError after
    // M114.
    case crosapi::mojom::DeskCrosApiError::kDeprecatedStorageError:
      return "StorageError";
    case crosapi::mojom::DeskCrosApiError::kNoCurrentUserError:
      return "NoCurrentUserError";
    case crosapi::mojom::DeskCrosApiError::kBadProfileError:
      return "BadProfileError";
    case crosapi::mojom::DeskCrosApiError::kResourceNotFoundError:
      return "ResourceNotFoundError";
    case crosapi::mojom::DeskCrosApiError::kInvalidIdError:
      return "InvalidIdError";
    case crosapi::mojom::DeskCrosApiError::kDesksBeingModifiedError:
      return "DeskBeingModifiedError";
    case crosapi::mojom::DeskCrosApiError::kDesksCountCheckFailedError:
      return "DesksCountCheckFailedError";
    case crosapi::mojom::DeskCrosApiError::kUnknownError:
      return "UnknownError";
  }
}

}  // namespace

WMDesksPrivateFeatureLacros::WMDesksPrivateFeatureLacros() = default;

WMDesksPrivateFeatureLacros::~WMDesksPrivateFeatureLacros() = default;

void WMDesksPrivateFeatureLacros::GetDeskTemplateJson(
    const base::GUID& template_uuid,
    Profile* profile,
    GetDeskTemplateJsonCallback callback) {
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::Desk>()) {
    std::move(callback).Run(kCROS_API_UNAVAILABLE, {});
    return;
  }
  service->GetRemote<crosapi::mojom::Desk>()->GetTemplateJson(
      template_uuid, base::BindOnce(
                         [](GetDeskTemplateJsonCallback callback,
                            crosapi::mojom::GetTemplateJsonResultPtr result) {
                           if (result->is_error()) {
                             std::move(callback).Run(
                                 GetStringError(result->get_error()), {});
                             return;
                           }
                           std::move(callback).Run(
                               {}, std::move(result->get_template_json()));
                         },
                         std::move(callback)));
}

void WMDesksPrivateFeatureLacros::LaunchDesk(std::string desk_name,
                                             LaunchDeskCallback callback) {
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::Desk>()) {
    std::move(callback).Run(kCROS_API_UNAVAILABLE, {});
    return;
  }
  service->GetRemote<crosapi::mojom::Desk>()->LaunchEmptyDesk(
      desk_name, base::BindOnce(
                     [](LaunchDeskCallback callback,
                        crosapi::mojom::LaunchEmptyDeskResultPtr result) {
                       if (result->is_error()) {
                         std::move(callback).Run(
                             GetStringError(result->get_error()), {});
                         return;
                       }
                       std::move(callback).Run({}, result->get_desk_id());
                     },
                     std::move(callback)));
}

void WMDesksPrivateFeatureLacros::RemoveDesk(const base::GUID& desk_uuid,
                                             bool close_all,
                                             RemoveDeskCallback callback) {
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::Desk>()) {
    std::move(callback).Run(kCROS_API_UNAVAILABLE);
    return;
  }
  service->GetRemote<crosapi::mojom::Desk>()->RemoveDesk(
      desk_uuid, close_all,
      base::BindOnce(
          [](RemoveDeskCallback callback,
             crosapi::mojom::RemoveDeskResultPtr result) {
            if (result->is_error()) {
              std::move(callback).Run(GetStringError(result->get_error()));
              return;
            }
            std::move(callback).Run({});
          },
          std::move(callback)));
}

void WMDesksPrivateFeatureLacros::SetAllDeskProperty(
    int window_id,
    bool all_desk,
    SetAllDeskPropertyCallback callback) {
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::Desk>()) {
    std::move(callback).Run(kCROS_API_UNAVAILABLE);
  }
  service->GetRemote<crosapi::mojom::Desk>()->SetAllDesksProperty(
      window_id, all_desk,
      base::BindOnce(
          [](SetAllDeskPropertyCallback callback,
             crosapi::mojom::SetAllDesksPropertyResultPtr result) {
            if (result->is_error()) {
              std::move(callback).Run(GetStringError(result->get_error()));
              return;
            }
            std::move(callback).Run({});
          },
          std::move(callback)));
}

void WMDesksPrivateFeatureLacros::GetAllDesks(GetAllDesksCallback callback) {
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::Desk>()) {
    std::move(callback).Run(kCROS_API_UNAVAILABLE, {});
    return;
  }
  service->GetRemote<crosapi::mojom::Desk>()->GetAllDesks(base::BindOnce(
      [](GetAllDesksCallback callback,
         crosapi::mojom::GetAllDesksResultPtr result) {
        if (result->is_error()) {
          std::move(callback).Run(GetStringError(result->get_error()), {});
          return;
        }
        std::vector<api::wm_desks_private::Desk> api_desks;
        for (const auto& desk : result->get_desks())
          api_desks.push_back(GetDeskFromCrosApiDesk(std::move(desk)));
        std::move(callback).Run({}, std::move(api_desks));
      },
      std::move(callback)));
}

void WMDesksPrivateFeatureLacros::SaveActiveDesk(
    SaveActiveDeskCallback callback) {
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::Desk>()) {
    std::move(callback).Run(kCROS_API_UNAVAILABLE, {});
    return;
  }
  service->GetRemote<crosapi::mojom::Desk>()->SaveActiveDesk(base::BindOnce(
      [](SaveActiveDeskCallback callback,
         crosapi::mojom::SaveActiveDeskResultPtr result) {
        if (result->is_error()) {
          std::move(callback).Run(GetStringError(result->get_error()), {});
          return;
        }
        api::wm_desks_private::SavedDesk saved_desk =
            GetSavedDeskFromCrosApiDesk(std::move(result->get_saved_desk()));
        std::move(callback).Run({}, std::move(saved_desk));
      },
      std::move(callback)));
}

void WMDesksPrivateFeatureLacros::DeleteSavedDesk(
    const base::GUID& desk_uuid,
    DeleteSavedDeskCallback callback) {
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::Desk>()) {
    std::move(callback).Run(kCROS_API_UNAVAILABLE);
    return;
  }
  service->GetRemote<crosapi::mojom::Desk>()->DeleteSavedDesk(
      desk_uuid,
      base::BindOnce(
          [](DeleteSavedDeskCallback callback,
             crosapi::mojom::DeleteSavedDeskResultPtr result) {
            if (result->is_error()) {
              std::move(callback).Run(GetStringError(result->get_error()));
              return;
            }
            std::move(callback).Run({});
          },
          std::move(callback)));
}

void WMDesksPrivateFeatureLacros::RecallSavedDesk(
    const base::GUID& desk_uuid,
    RecallSavedDeskCallback callback) {
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::Desk>()) {
    std::move(callback).Run(kCROS_API_UNAVAILABLE, {});
    return;
  }
  service->GetRemote<crosapi::mojom::Desk>()->RecallSavedDesk(
      desk_uuid, base::BindOnce(
                     [](RecallSavedDeskCallback callback,
                        crosapi::mojom::RecallSavedDeskResultPtr result) {
                       if (result->is_error()) {
                         std::move(callback).Run(
                             GetStringError(result->get_error()), {});
                         return;
                       }
                       std::move(callback).Run({}, result->get_desk_id());
                     },
                     std::move(callback)));
}

void WMDesksPrivateFeatureLacros::GetSavedDesks(
    GetSavedDesksCallback callback) {
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::Desk>() ||
      service->GetInterfaceVersion(crosapi::mojom::Desk::Uuid_) <
          static_cast<int>(crosapi::mojom::Desk::MethodMinVersions::
                               kGetSavedDesksMinVersion)) {
    std::move(callback).Run(kCROS_API_UNAVAILABLE, {});
    return;
  }
  service->GetRemote<crosapi::mojom::Desk>()->GetSavedDesks(base::BindOnce(
      [](GetSavedDesksCallback callback,
         crosapi::mojom::GetSavedDesksResultPtr result) {
        if (result->is_error()) {
          std::move(callback).Run(GetStringError(result->get_error()), {});
          return;
        }
        std::vector<api::wm_desks_private::SavedDesk> saved_desks;
        for (const auto& saved_desk : result->get_saved_desks())
          saved_desks.push_back(
              GetSavedDeskFromCrosApiSavedDesk(std::move(saved_desk)));
        std::move(callback).Run({}, std::move(saved_desks));
      },
      std::move(callback)));
}

void WMDesksPrivateFeatureLacros::GetActiveDesk(
    GetActiveDeskCallback callback) {
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::Desk>() ||
      service->GetInterfaceVersion(crosapi::mojom::Desk::Uuid_) <
          static_cast<int>(crosapi::mojom::Desk::MethodMinVersions::
                               kGetActiveDeskMinVersion)) {
    std::move(callback).Run(kCROS_API_UNAVAILABLE, {});
    return;
  }
  service->GetRemote<crosapi::mojom::Desk>()->GetActiveDesk(base::BindOnce(
      [](GetActiveDeskCallback callback,
         crosapi::mojom::GetActiveDeskResultPtr result) {
        if (result->is_error()) {
          std::move(callback).Run(GetStringError(result->get_error()), {});
          return;
        }
        std::move(callback).Run({}, result->get_desk_id());
      },
      std::move(callback)));
}

void WMDesksPrivateFeatureLacros::SwitchDesk(const base::GUID& desk_uuid,
                                             SwitchDeskCallback callback) {
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::Desk>() ||
      service->GetInterfaceVersion(crosapi::mojom::Desk::Uuid_) <
          static_cast<int>(
              crosapi::mojom::Desk::MethodMinVersions::kSwitchDeskMinVersion)) {
    std::move(callback).Run(kCROS_API_UNAVAILABLE);
    return;
  }
  service->GetRemote<crosapi::mojom::Desk>()->SwitchDesk(
      desk_uuid,
      base::BindOnce(
          [](SwitchDeskCallback callback,
             crosapi::mojom::SwitchDeskResultPtr result) {
            if (result->is_error()) {
              std::move(callback).Run(GetStringError(result->get_error()));
              return;
            }
            std::move(callback).Run({});
          },
          std::move(callback)));
}

}  // namespace extensions
