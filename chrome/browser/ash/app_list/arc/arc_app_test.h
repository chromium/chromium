// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_APP_TEST_H_
#define CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_APP_TEST_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/components/arc/mojom/app.mojom-forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/scoped_user_manager.h"

namespace arc {
namespace mojom {
class AppInfo;
}
class ArcPlayStoreEnabledPreferenceHandler;
class ArcServiceManager;
class ArcSessionManager;
class FakeAppInstance;
class FakeCompatibilityModeInstance;
class FakeIntentHelperHost;
class FakeIntentHelperInstance;
}  // namespace arc

namespace user_manager {
class User;
}

class ArcAppListPrefs;
class Profile;

// Helper class to initialize arc bridge to work with arc apps in unit tests.
class ArcAppTest {
 public:
  enum UserManagerMode {
    kDoNothing,
    kCreate,
  };
  explicit ArcAppTest(
      UserManagerMode user_manager_mode = UserManagerMode::kCreate);

  ArcAppTest(const ArcAppTest&) = delete;
  ArcAppTest& operator=(const ArcAppTest&) = delete;

  virtual ~ArcAppTest();

  void SetUp(Profile* profile);
  void TearDown();

  // Public methods to modify AppInstance for unit_tests.
  void StopArcInstance();
  void RestartArcInstance();

  void SetUpIntentHelper();

  static std::string GetAppId(const arc::mojom::AppInfo& app_info);
  static std::string GetAppId(const arc::mojom::ShortcutInfo& shortcut);

  // Helper that clones packages info array.
  static std::vector<arc::mojom::ArcPackageInfoPtr> ClonePackages(
      const std::vector<arc::mojom::ArcPackageInfoPtr>& packages);

  // Helper that clones app info array.
  static std::vector<arc::mojom::AppInfoPtr> CloneApps(
      const std::vector<arc::mojom::AppInfoPtr>& apps);

  const std::vector<arc::mojom::ArcPackageInfoPtr>& fake_packages() const {
    return fake_packages_;
  }

  // Adds package info and takes ownership.
  void AddPackage(arc::mojom::ArcPackageInfoPtr package);

  // Replaces package if a package with this package name exists.
  void UpdatePackage(arc::mojom::ArcPackageInfoPtr package);

  void RemovePackage(const std::string& package_name);

  void WaitForDefaultApps();
  void WaitForRemoveAllApps();

  // The 0th item is sticky but not the followings.
  const std::vector<arc::mojom::AppInfoPtr>& fake_apps() const {
    return fake_apps_;
  }

  const std::vector<arc::mojom::AppInfoPtr>& fake_default_apps() const {
    return fake_default_apps_;
  }

  const std::vector<arc::mojom::ShortcutInfo>& fake_shortcuts() const {
    return fake_shortcuts_;
  }

  arc::FakeAppInstance* app_instance() { return app_instance_.get(); }

  arc::FakeCompatibilityModeInstance* compatibility_mode_instance() {
    return compatibility_mode_instance_.get();
  }

  arc::FakeIntentHelperInstance* intent_helper_instance() {
    return intent_helper_instance_.get();
  }

  ArcAppListPrefs* arc_app_list_prefs() { return arc_app_list_pref_; }

  arc::ArcSessionManager* arc_session_manager() {
    return arc_session_manager_.get();
  }
  arc::ArcServiceManager* arc_service_manager() {
    return arc_service_manager_.get();
  }

  void set_wait_default_apps(bool wait_default_apps) {
    wait_default_apps_ = wait_default_apps;
  }

  void set_activate_arc_on_start(bool activate_arc_on_start) {
    activate_arc_on_start_ = activate_arc_on_start;
  }

  void set_persist_service_manager(bool persist_service_manager) {
    persist_service_manager_ = persist_service_manager;
  }

  void set_start_app_service_publisher(bool start_app_service_publisher) {
    start_app_service_publisher_ = start_app_service_publisher;
  }

  void set_initialize_real_intent_helper_bridge(bool value) {
    initialize_real_intent_helper_bridge_ = value;
  }

  void set_wait_compatibility_mode(bool value) {
    wait_compatibility_mode_ = value;
  }

 private:
  const user_manager::User* CreateUserAndLogin();
  bool FindPackage(const std::string& package_name);
  void CreateFakeAppsAndPackages();

  // Unowned pointer.
  raw_ptr<Profile> profile_ = nullptr;

  raw_ptr<ArcAppListPrefs, DanglingUntriaged> arc_app_list_pref_ = nullptr;

  bool wait_default_apps_ = true;

  // If set to true ARC would be automatically enabled on test start up.
  bool activate_arc_on_start_ = true;

  // Whether arc service manager should be destroyed when this object gets torn
  // down.
  bool persist_service_manager_ = false;

  // Whether the ArcApps AppService publisher should be started during
  // initialization.
  bool start_app_service_publisher_ = true;

  // If set to true, the real ArcIntentHelperBridge is initialized on test start
  // up.
  bool initialize_real_intent_helper_bridge_ = false;

  bool wait_compatibility_mode_ = false;

  std::unique_ptr<session_manager::SessionManager> session_manager_;

  std::unique_ptr<arc::ArcServiceManager> arc_service_manager_;
  std::unique_ptr<arc::ArcSessionManager> arc_session_manager_;
  std::unique_ptr<arc::ArcPlayStoreEnabledPreferenceHandler>
      arc_play_store_enabled_preference_handler_;
  std::unique_ptr<arc::FakeAppInstance> app_instance_;
  std::unique_ptr<arc::FakeCompatibilityModeInstance>
      compatibility_mode_instance_;
  std::unique_ptr<arc::FakeIntentHelperHost> intent_helper_host_;
  std::unique_ptr<arc::FakeIntentHelperInstance> intent_helper_instance_;

  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  std::vector<arc::mojom::AppInfoPtr> fake_apps_;
  std::vector<arc::mojom::AppInfoPtr> fake_default_apps_;
  std::vector<arc::mojom::ArcPackageInfoPtr> fake_packages_;
  std::vector<arc::mojom::ShortcutInfo> fake_shortcuts_;

  bool concierge_client_initialized_ = false;
};

#endif  // CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_APP_TEST_H_
