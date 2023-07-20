// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_WM_WM_DESKS_PRIVATE_FEATURE_ASH_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_WM_WM_DESKS_PRIVATE_FEATURE_ASH_H_

#include <string>

#include "base/uuid.h"
#include "chrome/browser/chromeos/extensions/wm/wm_desks_private_feature.h"

class Profile;

namespace extensions {

// This is ash implementation of wm_desks_private_feature.h. It's implemented by
// directly talking to DesksClient.
class WMDesksPrivateFeatureAsh : public WMDesksPrivateFeature {
 public:
  WMDesksPrivateFeatureAsh();
  ~WMDesksPrivateFeatureAsh() override;

  void GetDeskTemplateJson(const base::Uuid& template_uuid,
                           Profile* profile,
                           GetDeskTemplateJsonCallback callback) override;

  void LaunchDesk(std::string desk_name, LaunchDeskCallback callback) override;

  void RemoveDesk(const base::Uuid& desk_uuid,
                  bool close_all,
                  bool allow_undo,
                  RemoveDeskCallback callback) override;

  void SetAllDeskProperty(int32_t window_id,
                          bool all_desk,
                          SetAllDeskPropertyCallback callback) override;

  void GetAllDesks(GetAllDesksCallback callback) override;

  void SaveActiveDesk(SaveActiveDeskCallback callback) override;

  void DeleteSavedDesk(const base::Uuid& desk_uuid,
                       DeleteSavedDeskCallback callback) override;

  void RecallSavedDesk(const base::Uuid& desk_uuid,
                       RecallSavedDeskCallback callback) override;

  void GetSavedDesks(GetSavedDesksCallback callback) override;

  void GetActiveDesk(GetActiveDeskCallback callback) override;

  void SwitchDesk(const base::Uuid& desk_uuid,
                  SwitchDeskCallback callback) override;
  void GetDeskByID(const base::Uuid& desk_uuid,
                   GetDeskByIDCallback callback) override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_WM_WM_DESKS_PRIVATE_FEATURE_ASH_H_
