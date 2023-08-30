// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_COMPANION_APP_COMPANION_APP_INSTALLER_H_
#define ASH_QUICK_PAIR_COMPANION_APP_COMPANION_APP_INSTALLER_H_

namespace ash {
namespace quick_pair {

// CompanionAppInstaller downloads and installs the device's companion app
class CompanionAppInstaller {
 public:
  CompanionAppInstaller();
  CompanionAppInstaller(const CompanionAppInstaller&) = delete;
  CompanionAppInstaller& operator=(const CompanionAppInstaller&) = delete;
  ~CompanionAppInstaller();
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_COMPANION_APP_COMPANION_APP_INSTALLER_H_
