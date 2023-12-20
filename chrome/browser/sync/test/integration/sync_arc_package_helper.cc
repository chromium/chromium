// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sync_arc_package_helper.h"

#include <vector>

#include "ash/components/arc/session/connection_holder.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_app_instance.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_package_syncable_service.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/sync/protocol/arc_package_specifics.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"

namespace arc {

namespace {

std::string GetTestPackageName(size_t id) {
  return "testarcpackage" + base::NumberToString(id);
}

}  // namespace

// static
SyncArcPackageHelper* SyncArcPackageHelper::GetInstance() {
  SyncArcPackageHelper* instance = base::Singleton<SyncArcPackageHelper>::get();
  DCHECK(sync_datatype_helper::test());
  instance->SetupTest(sync_datatype_helper::test());
  return instance;
}

// static
sync_pb::EntitySpecifics SyncArcPackageHelper::GetTestSpecifics(size_t id) {
  sync_pb::EntitySpecifics specifics;
  sync_pb::ArcPackageSpecifics* arc_specifics = specifics.mutable_arc_package();
  arc_specifics->set_package_name(GetTestPackageName(id));
  arc_specifics->set_package_version(id);
  arc_specifics->set_last_backup_android_id(id);
  arc_specifics->set_last_backup_time(id);
  return specifics;
}

SyncArcPackageHelper::SyncArcPackageHelper() = default;

SyncArcPackageHelper::~SyncArcPackageHelper() = default;

void SyncArcPackageHelper::SetupTest(SyncTest* test) {
  if (setup_completed_) {
    DCHECK_EQ(test, test_);
    return;
  }
  test_ = test;

  for (Profile* profile : test_->GetAllProfiles()) {
    EnableArcService(profile);
    SendRefreshPackageList(profile);
  }

  setup_completed_ = true;
}

void SyncArcPackageHelper::InstallPackageWithIndex(Profile* profile,
                                                   size_t id) {
  std::string package_name = GetTestPackageName(id);

  mojom::ArcPackageInfo package;
  package.package_name = package_name;
  package.package_version = id;
  package.last_backup_android_id = id;
  package.last_backup_time = id;
  package.sync = true;

  InstallPackage(profile, package);
}

void SyncArcPackageHelper::UninstallPackageWithIndex(Profile* profile,
                                                     size_t id) {
  std::string package_name = GetTestPackageName(id);
  UninstallPackage(profile, package_name);
}

void SyncArcPackageHelper::ClearPackages(Profile* profile) {
  const ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile);
  DCHECK(prefs);
  const std::vector<std::string> pref_packages = prefs->GetPackagesFromPrefs();
  for (const std::string& package : pref_packages) {
    UninstallPackage(profile, package);
  }
}

bool SyncArcPackageHelper::HasOnlyTestPackages(Profile* profile,
                                               const std::vector<size_t>& ids) {
  const ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile);
  DCHECK(prefs);
  if (prefs->GetPackagesFromPrefs().size() != ids.size()) {
    return false;
  }

  for (const size_t id : ids) {
    std::unique_ptr<ArcAppListPrefs::PackageInfo> package_info =
        prefs->GetPackage(GetTestPackageName(id));
    if (!package_info) {
      return false;
    }
    // See InstallPackageWithIndex().
    if (package_info->package_version != static_cast<int32_t>(id) ||
        package_info->last_backup_android_id != static_cast<int64_t>(id) ||
        package_info->last_backup_time != static_cast<int64_t>(id) ||
        !package_info->should_sync) {
      return false;
    }
  }

  return true;
}

bool SyncArcPackageHelper::AllProfilesHaveSamePackages() {
  const std::vector<raw_ptr<Profile, VectorExperimental>>& profiles =
      test_->GetAllProfiles();
  for (Profile* profile : profiles) {
    if (profile != profiles.front() &&
        !ArcPackagesMatch(profiles.front(), profile)) {
      DVLOG(1) << "Packages match failed!";
      return false;
    }
  }
  return true;
}

bool SyncArcPackageHelper::AllProfilesHaveSamePackageDetails() {
  if (!AllProfilesHaveSamePackages()) {
    DVLOG(1) << "Packages match failed, skip packages detail match.";
    return false;
  }

  const std::vector<raw_ptr<Profile, VectorExperimental>>& profiles =
      test_->GetAllProfiles();
  for (Profile* profile : profiles) {
    if (profile != profiles.front() &&
        !ArcPackageDetailsMatch(profiles.front(), profile)) {
      DVLOG(1) << "Profile1: " << ArcPackageSyncableService::Get(profile);
      DVLOG(1) << "Profile2: "
               << ArcPackageSyncableService::Get(profiles.front());
      return false;
    }
  }
  return true;
}

void SyncArcPackageHelper::EnableArcService(Profile* profile) {
  DCHECK(profile);
  DCHECK(!instance_map_[profile]);

  arc::SetArcPlayStoreEnabledForProfile(profile, true);
  // Usually ArcPlayStoreEnabledPreferenceHandler would take care of propagating
  // prefs changes to observers, but that's not the case in integration tests.
  arc::ArcSessionManager::Get()->NotifyArcPlayStoreEnabledChanged(true);

  ArcAppListPrefs* arc_app_list_prefs = ArcAppListPrefs::Get(profile);
  DCHECK(arc_app_list_prefs);

  base::RunLoop run_loop;
  arc_app_list_prefs->SetDefaultAppsReadyCallback(run_loop.QuitClosure());
  run_loop.Run();

  instance_map_[profile] =
      std::make_unique<FakeAppInstance>(arc_app_list_prefs);
  DCHECK(instance_map_[profile].get());
  arc_app_list_prefs->app_connection_holder()->SetInstance(
      instance_map_[profile].get());
  WaitForInstanceReady(arc_app_list_prefs->app_connection_holder());
}

void SyncArcPackageHelper::SendRefreshPackageList(Profile* profile) {
  // OnPackageListRefreshed will be called when AppInstance is ready.
  // For fakeAppInstance we use SendRefreshPackageList to make sure that
  // OnPackageListRefreshed will be called.
  instance_map_[profile]->SendRefreshPackageList({});
}

void SyncArcPackageHelper::DisableArcService(Profile* profile) {
  DCHECK(profile);
  DCHECK(instance_map_[profile]);

  arc::SetArcPlayStoreEnabledForProfile(profile, false);
  // Usually ArcPlayStoreEnabledPreferenceHandler would take care of propagating
  // prefs changes to observers, but that's not the case in integration tests.
  arc::ArcSessionManager::Get()->NotifyArcPlayStoreEnabledChanged(false);

  ArcAppListPrefs* arc_app_list_prefs = ArcAppListPrefs::Get(profile);
  DCHECK(arc_app_list_prefs);

  arc_app_list_prefs->app_connection_holder()->CloseInstance(
      instance_map_[profile].get());
  instance_map_.erase(profile);
}

void SyncArcPackageHelper::InstallPackage(
    Profile* profile,
    const mojom::ArcPackageInfo& package) {
  ArcAppListPrefs* arc_app_list_prefs = ArcAppListPrefs::Get(profile);
  DCHECK(arc_app_list_prefs);
  mojom::AppInstance* app_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_app_list_prefs->app_connection_holder(), InstallPackage);

  DCHECK(app_instance);
  // After this function, new package should be added to local sync service
  // and install event should be sent to sync server.
  app_instance->InstallPackage(package.Clone());
}

void SyncArcPackageHelper::UninstallPackage(Profile* profile,
                                            const std::string& package_name) {
  ArcAppListPrefs* arc_app_list_prefs = ArcAppListPrefs::Get(profile);
  DCHECK(arc_app_list_prefs);
  mojom::AppInstance* app_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_app_list_prefs->app_connection_holder(), UninstallPackage);
  DCHECK(app_instance);
  // After this function, package should be removed from local sync service
  // and uninstall event should be sent to sync server.
  app_instance->UninstallPackage(package_name);
}

// Packages from local pref are used for these test functions. Packages in local
// pref should be indentical to syncservice after syncservice is launched.
// Packagd update behavior is not synced by design.
bool SyncArcPackageHelper::ArcPackagesMatch(Profile* profile1,
                                            Profile* profile2) {
  const ArcAppListPrefs* prefs1 = ArcAppListPrefs::Get(profile1);
  const ArcAppListPrefs* prefs2 = ArcAppListPrefs::Get(profile2);
  DCHECK(prefs1);
  DCHECK(prefs2);
  const std::vector<std::string> pref1_packages =
      prefs1->GetPackagesFromPrefs();
  const std::vector<std::string> pref2_packages =
      prefs2->GetPackagesFromPrefs();
  if (pref1_packages.size() != pref2_packages.size()) {
    return false;
  }
  for (const std::string& package : pref1_packages) {
    std::unique_ptr<ArcAppListPrefs::PackageInfo> package_info =
        prefs2->GetPackage(package);
    if (!package_info.get()) {
      return false;
    }
  }
  return true;
}

bool SyncArcPackageHelper::ArcPackageDetailsMatch(Profile* profile1,
                                                  Profile* profile2) {
  const ArcAppListPrefs* prefs1 = ArcAppListPrefs::Get(profile1);
  const ArcAppListPrefs* prefs2 = ArcAppListPrefs::Get(profile2);
  DCHECK(prefs1);
  DCHECK(prefs2);
  const std::vector<std::string> pref1_packages =
      prefs1->GetPackagesFromPrefs();
  for (const std::string& package : pref1_packages) {
    std::unique_ptr<ArcAppListPrefs::PackageInfo> package1_info =
        prefs1->GetPackage(package);
    std::unique_ptr<ArcAppListPrefs::PackageInfo> package2_info =
        prefs2->GetPackage(package);
    if (!package2_info.get()) {
      return false;
    }
    if (package1_info->package_version != package2_info->package_version) {
      return false;
    }
    if (package1_info->last_backup_android_id !=
        package2_info->last_backup_android_id) {
      return false;
    }
    if (package1_info->last_backup_time != package2_info->last_backup_time) {
      return false;
    }
  }
  return true;
}

}  // namespace arc
