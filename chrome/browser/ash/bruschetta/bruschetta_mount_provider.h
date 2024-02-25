// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_MOUNT_PROVIDER_H_
#define CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_MOUNT_PROVIDER_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/bruschetta/bruschetta_launcher.h"
#include "chrome/browser/ash/guest_os/public/guest_os_mount_provider.h"

class Profile;

namespace bruschetta {

class BruschettaMountProvider : public guest_os::GuestOsMountProvider {
 public:
  BruschettaMountProvider(Profile* profile, guest_os::GuestId guest_id);
  ~BruschettaMountProvider() override;

  // guest_os::GuestOsMountProvider overrides.
  Profile* profile() override;
  std::string DisplayName() override;
  guest_os::GuestId GuestId() override;
  guest_os::VmType vm_type() override;
  std::unique_ptr<guest_os::GuestOsFileWatcher> CreateFileWatcher(
      base::FilePath mount_path,
      base::FilePath relative_path) override;

 protected:
  // guest_os::GuestOsMountProvider override.
  void Prepare(PrepareCallback callback) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(BruschettaMountProviderTest, TestPrepare);
  FRIEND_TEST_ALL_PREFIXES(BruschettaMountProviderTest,
                           TestPrepareLaunchFailure);
  void OnRunning(PrepareCallback callback, BruschettaResult result);
  raw_ptr<Profile> profile_;
  guest_os::GuestId guest_id_;
  base::CallbackListSubscription unmount_subscription_;
  base::WeakPtrFactory<BruschettaMountProvider> weak_ptr_factory_{this};
};

}  // namespace bruschetta

#endif  // CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_MOUNT_PROVIDER_H_
