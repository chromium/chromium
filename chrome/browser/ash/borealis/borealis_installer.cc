// Copyright 2020 The Chromium Authors. All rights reserved.
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
    case InstallingState::kInstallingDlc:
      return "kInstallingDlc";
  }
}

}  // namespace borealis
