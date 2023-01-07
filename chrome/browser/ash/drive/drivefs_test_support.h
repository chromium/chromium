// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DRIVE_DRIVEFS_TEST_SUPPORT_H_
#define CHROME_BROWSER_ASH_DRIVE_DRIVEFS_TEST_SUPPORT_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "chromeos/ash/components/drivefs/drivefs_host.h"
#include "chromeos/ash/components/drivefs/fake_drivefs.h"

class Profile;

namespace drive {

bool SetUpUserDataDirectoryForDriveFsTest();

class FakeDriveFsHelper {
 public:
  static const char kPredefinedProfileSalt[];

  FakeDriveFsHelper(Profile* profile, const base::FilePath& mount_path);

  FakeDriveFsHelper(const FakeDriveFsHelper&) = delete;
  FakeDriveFsHelper& operator=(const FakeDriveFsHelper&) = delete;

  ~FakeDriveFsHelper();

  base::RepeatingCallback<std::unique_ptr<drivefs::DriveFsBootstrapListener>()>
  CreateFakeDriveFsListenerFactory();

  const base::FilePath& mount_path() { return mount_path_; }
  drivefs::FakeDriveFs& fake_drivefs() { return fake_drivefs_; }

 private:
  const base::FilePath mount_path_;
  drivefs::FakeDriveFs fake_drivefs_;
};

}  // namespace drive

#endif  // CHROME_BROWSER_ASH_DRIVE_DRIVEFS_TEST_SUPPORT_H_
