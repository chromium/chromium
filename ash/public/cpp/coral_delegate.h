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

  // Moves the `tabs` from the desk with the given `src_desk_index` to a browser
  // on the new desk.
  virtual void MoveTabsInGroupToNewDesk(
      const std::vector<coral::mojom::Tab>& tabs,
      size_t src_desk_index) = 0;

  // The default restore Id for chrome browser is under chrome/browser/. This
  // lets us get the correct Id in ash/.
  virtual int GetChromeDefaultRestoreId() = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_CORAL_DELEGATE_H_
