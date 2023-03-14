// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/ui/actions.h"

namespace ash {
namespace quick_pair {

std::ostream& operator<<(std::ostream& stream, DiscoveryAction action) {
  switch (action) {
    case DiscoveryAction::kPairToDevice:
      stream << "[Pair to device]";
      break;
    case DiscoveryAction::kDismissedByUser:
      stream << "[Dismissed by user]";
      break;
    case DiscoveryAction::kDismissedByOs:
      stream << "[Dismissed by OS]";
      break;
    case DiscoveryAction::kLearnMore:
      stream << "[Learn more]";
      break;
    case DiscoveryAction::kDismissedByTimeout:
      stream << "[Dismissed by timeout]";
      break;
  }

  return stream;
}

std::ostream& operator<<(std::ostream& stream, AssociateAccountAction action) {
  switch (action) {
    case AssociateAccountAction::kAssociateAccount:
      stream << "[Associate account]";
      break;
    case AssociateAccountAction::kLearnMore:
      stream << "[Learn more]";
      break;
    case AssociateAccountAction::kDismissedByUser:
      stream << "[Dismissed by user]";
      break;
    case AssociateAccountAction::kDismissedByOs:
      stream << "[Dismissed by OS]";
      break;
    case AssociateAccountAction::kDismissedByTimeout:
      stream << "[Dismissed by timeout]";
      break;
  }

  return stream;
}

std::ostream& operator<<(std::ostream& stream, CompanionAppAction action) {
  switch (action) {
    case CompanionAppAction::kDownloadAndLaunchApp:
      stream << "[Download and launch app]";
      break;
    case CompanionAppAction::kLaunchApp:
      stream << "[Launch app]";
      break;
    case CompanionAppAction::kDismissedByUser:
      stream << "[Dismissed by user]";
      break;
    case CompanionAppAction::kDismissed:
      stream << "[Dismissed]";
      break;
  }

  return stream;
}

std::ostream& operator<<(std::ostream& stream, PairingFailedAction action) {
  switch (action) {
    case PairingFailedAction::kNavigateToSettings:
      stream << "[Navigate to settings]";
      break;
    case PairingFailedAction::kDismissedByUser:
      stream << "[Dismissed by user]";
      break;
    case PairingFailedAction::kDismissed:
      stream << "[Dismissed]";
      break;
  }

  return stream;
}

}  // namespace quick_pair
}  // namespace ash
