// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_installer.h"

namespace borealis {

BorealisInstaller::BorealisInstaller() = default;

BorealisInstaller::~BorealisInstaller() = default;

// static
std::string BorealisInstaller::GetInstallingStateName(InstallingState state) {
  switch (state) {
    case InstallingState::kInactive:
      return "kInactive";
    case InstallingState::kCheckingIfAllowed:
      return "kCheckingIfAllowed";
    case InstallingState::kInstallingDlc:
      return "kInstallingDlc";
    case InstallingState::kStartingUp:
      return "kStartingUp";
    case InstallingState::kAwaitingApplications:
      return "kAwaitingApplications";
  }
}

}  // namespace borealis
