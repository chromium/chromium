// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_DRIVEFS_TEST_SUPPORT_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_DRIVEFS_TEST_SUPPORT_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "chromeos/components/drivefs/drivefs_host.h"
#include "chromeos/components/drivefs/fake_drivefs.h"

class Profile;

namespace drive {

bool SetUpUserDataDirectoryForDriveFsTest();

class FakeDriveFsHelper {
 public:
  static const char kPredefinedProfileSalt[];

  FakeDriveFsHelper(Profile* profile, const base::FilePath& mount_path);
  ~FakeDriveFsHelper();

  base::RepeatingCallback<
      std::unique_ptr<drivefs::DriveFsHost::MojoConnectionDelegate>()>
  CreateFakeDriveFsConnectionDelegateFactory();

  const base::FilePath& mount_path() { return mount_path_; }
  drivefs::FakeDriveFs& fake_drivefs() { return fake_drivefs_; }

 private:
  const base::FilePath mount_path_;
  drivefs::FakeDriveFs fake_drivefs_;

  DISALLOW_COPY_AND_ASSIGN(FakeDriveFsHelper);
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_DRIVEFS_TEST_SUPPORT_H_
