// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_WM_WM_DESKS_PRIVATE_FEATURE_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_WM_WM_DESKS_PRIVATE_FEATURE_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/uuid.h"
#include "base/values.h"
#include "chromeos/ash/experiences/extensions/common/api/wm_desks_private.h"

class Profile;

namespace extensions {

// This class is the implementation for wm_desks_private feature. It's
// implemented by directly talking to DesksClient.
class WMDesksPrivateFeature {
 public:
  WMDesksPrivateFeature();
  ~WMDesksPrivateFeature();

  using GetDeskTemplateJsonCallback =
      base::OnceCallback<void(std::string, const base::Value)>;
  // Gets the template associated with the templateUuid and returns its JSON
  // representation.
  void GetDeskTemplateJson(const base::Uuid& template_uuid,
                           Profile* profile,
                           GetDeskTemplateJsonCallback callback);

  using LaunchDeskCallback =
      base::OnceCallback<void(std::string, const base::Uuid&)>;

  // Launches a desk with provided `desk_name` and returns `desk_uuid` of new
  // desk.
  void LaunchDesk(std::string desk_name, LaunchDeskCallback callback);

  using RemoveDeskCallback = base::OnceCallback<void(std::string)>;
  // Removes a desk as specified in `desk_uuid`. If `combine_desk` is present or
  // set to true, remove the desk and combine windows to the active desk to the
  // left. Otherwise close all windows on the desk. Provide user with
  // notification asking to undo removal if `allow_undo` is present or true.
  void RemoveDesk(const base::Uuid& desk_uuid,
                  bool combine_desk,
                  bool allow_undo,
                  RemoveDeskCallback callback);

  using SetAllDeskPropertyCallback = base::OnceCallback<void(std::string)>;
  // Sets the window properties for window identified by the `windowId`.
  void SetAllDeskProperty(int32_t window_id,
                          bool all_desk,
                          SetAllDeskPropertyCallback callback);

  using GetAllDesksCallback =
      base::OnceCallback<void(std::string,
                              std::vector<api::wm_desks_private::Desk>)>;
  // Returns all available desks.
  void GetAllDesks(GetAllDesksCallback callback);

  using SaveActiveDeskCallback =
      base::OnceCallback<void(std::string,
                              api::wm_desks_private::SavedDesk saved_desk)>;
  // Saves the current active desk to the library and remove it from the desk
  // bar.
  void SaveActiveDesk(SaveActiveDeskCallback callback);

  using DeleteSavedDeskCallback = base::OnceCallback<void(std::string)>;
  // Deletes the saved desk from the library.
  void DeleteSavedDesk(const base::Uuid& desk_uuid,
                       DeleteSavedDeskCallback callback);

  using RecallSavedDeskCallback =
      base::OnceCallback<void(std::string, const base::Uuid&)>;
  // Launches a saved desk from the library back to active desk.
  void RecallSavedDesk(const base::Uuid& desk_uuid,
                       RecallSavedDeskCallback callback);

  using GetSavedDesksCallback =
      base::OnceCallback<void(std::string,
                              std::vector<api::wm_desks_private::SavedDesk>)>;
  // List saved desks from the library.
  void GetSavedDesks(GetSavedDesksCallback callback);

  using GetActiveDeskCallback =
      base::OnceCallback<void(std::string, const base::Uuid&)>;
  // Retrieves the UUID of the current active desk.
  void GetActiveDesk(GetActiveDeskCallback callback);

  using SwitchDeskCallback = base::OnceCallback<void(std::string)>;
  // Switches to the target desk.
  void SwitchDesk(const base::Uuid& desk_uuid, SwitchDeskCallback callback);

  using GetDeskByIDCallback =
      base::OnceCallback<void(std::string, api::wm_desks_private::Desk desk)>;
  // Retrieves the desk information by desk ID.
  void GetDeskByID(const base::Uuid& desk_uuid, GetDeskByIDCallback callback);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_WM_WM_DESKS_PRIVATE_FEATURE_H_
