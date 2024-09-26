// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_CORAL_DELEGATE_H_
#define ASH_PUBLIC_CPP_CORAL_DELEGATE_H_

#include "ash/public/cpp/ash_public_export.h"
#include "chromeos/ash/services/coral/public/mojom/coral_service.mojom.h"

namespace ash {

// This delegate is owned by Shell and used by ash/ to communicate with
// `CoralClient` in chrome/ for browser operations.
class ASH_PUBLIC_EXPORT CoralDelegate {
 public:
  virtual ~CoralDelegate() = default;

  // Creates up to one browser with tabs from `group`. Launches the apps given
  // the app ids in `group`. This should be called from the post-login
  // overview screen and there should be no open applications. See
  // ash/wm/window_restore/README.md for more details about post-login.
  virtual void LaunchPostLoginGroup(coral::mojom::GroupPtr group) = 0;

  // Moves the tabs from `group` to a browser on the new desk.
  virtual void MoveTabsInGroupToNewDesk(coral::mojom::GroupPtr group) = 0;

  // Creates a saved desk with up to one browser with tabs from `group`.
  // Closes apps based on the apps from `group`, and places them in the saved
  // desk to be launched at a later time.
  virtual void CreateSavedDeskFromGroup(coral::mojom::GroupPtr group) = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_CORAL_DELEGATE_H_
