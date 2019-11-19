// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_ARC_PACKAGE_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_ARC_PACKAGE_HELPER_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/arc/mojom/app.mojom.h"
#include "components/sync/protocol/sync.pb.h"

class Profile;
class SyncTest;

namespace arc {
class FakeAppInstance;
}

namespace arc {

class SyncArcPackageHelper {
 public:
  static SyncArcPackageHelper* GetInstance();

  static sync_pb::EntitySpecifics GetTestSpecifics(size_t id);

  void SetupTest(SyncTest* test);

  void InstallPackageWithIndex(Profile* profile, size_t id);

  void UninstallPackageWithIndex(Profile* profile, size_t id);

  void ClearPackages(Profile* profile);

  bool AllProfilesHaveSamePackages();

  bool AllProfilesHaveSamePackageDetails();

  void EnableArcService(Profile* profile);

  void DisableArcService(Profile* profile);

  void SendRefreshPackageList(Profile* profile);

 private:
  friend struct base::DefaultSingletonTraits<SyncArcPackageHelper>;

  SyncArcPackageHelper();
  ~SyncArcPackageHelper();

  void InstallPackage(Profile* profile, const mojom::ArcPackageInfo& package);

  void UninstallPackage(Profile* profile, const std::string& package_name);

  // Returns true if |profile1| has the same arc packages as |profile2|.
  bool ArcPackagesMatch(Profile* profile1, Profile* profile2);

  // Returns true if |profile1| has the same arc packages and detail package
  // informaton as |profile2|.
  bool ArcPackageDetailsMatch(Profile* profile1, Profile* profile2);

  SyncTest* test_;
  bool setup_completed_;

  std::unordered_map<Profile*, std::unique_ptr<FakeAppInstance>> instance_map_;

  DISALLOW_COPY_AND_ASSIGN(SyncArcPackageHelper);
};

}  // namespace arc

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_ARC_PACKAGE_HELPER_H_
