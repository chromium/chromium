// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_NOTIFIER_SETTINGS_OBSERVER_H_
#define ASH_PUBLIC_CPP_NOTIFIER_SETTINGS_OBSERVER_H_

#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "base/observer_list_types.h"

namespace message_center {
struct NotifierId;
}

namespace gfx {
class ImageSkia;
}

namespace ash {

struct NotifierMetadata;

// An interface used to listen for changes to notifier settings information,
// implemented by any view that displays info about notifiers.
class ASH_PUBLIC_EXPORT NotifierSettingsObserver
    : public base::CheckedObserver {
 public:
  // Sets the user-visible and toggle-able list of notifiers.
  virtual void OnNotifiersUpdated(
      const std::vector<NotifierMetadata>& notifiers) {}

  // Updates an icon for a notifier previously sent via OnNotifierListUpdated.
  virtual void OnNotifierIconUpdated(
      const message_center::NotifierId& notifier_id,
      const gfx::ImageSkia& icon) {}
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_NOTIFIER_SETTINGS_OBSERVER_H_
