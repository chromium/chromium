// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_UI_ACTIONS_H_
#define ASH_QUICK_PAIR_UI_ACTIONS_H_

#include <ostream>
#include "base/component_export.h"

namespace ash {
namespace quick_pair {

enum class COMPONENT_EXPORT(QUICK_PAIR_UI) DiscoveryAction {
  kPairToDevice = 0,
  kDismissedByUser = 1,
  kDismissed = 2
};

enum class COMPONENT_EXPORT(QUICK_PAIR_UI) AssociateAccountAction {
  kAssoicateAccount = 0,
  kLearnMore = 1,
  kDismissedByUser = 2,
  kDismissed = 3
};

enum class COMPONENT_EXPORT(QUICK_PAIR_UI) CompanionAppAction {
  kDownloadAndLaunchApp = 0,
  kLaunchApp = 1,
  kDismissedByUser = 2,
  kDismissed = 3
};

enum class COMPONENT_EXPORT(QUICK_PAIR_UI) PairingFailedAction {
  kNavigateToSettings = 0,
  kDismissedByUser = 1,
  kDismissed = 2
};

COMPONENT_EXPORT(QUICK_PAIR_UI)
std::ostream& operator<<(std::ostream& stream, DiscoveryAction protocol);

COMPONENT_EXPORT(QUICK_PAIR_UI)
std::ostream& operator<<(std::ostream& stream, AssociateAccountAction protocol);

COMPONENT_EXPORT(QUICK_PAIR_UI)
std::ostream& operator<<(std::ostream& stream, CompanionAppAction protocol);

COMPONENT_EXPORT(QUICK_PAIR_UI)
std::ostream& operator<<(std::ostream& stream, PairingFailedAction protocol);

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_UI_ACTIONS_H_
