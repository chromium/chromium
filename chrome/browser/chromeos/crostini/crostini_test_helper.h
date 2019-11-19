// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_TEST_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_TEST_HELPER_H_

#include <map>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chromeos/dbus/vm_applications/apps.pb.h"

class TestingProfile;

namespace user_manager {
class ScopedUserManager;
}  // namespace user_manager

namespace crostini {

// This class is used to help test Crostini app integration by providing a
// simple interface to add, update, and remove apps from the registry.
class CrostiniTestHelper {
 public:
  // For convenience, instantiating this allows Crostini, and enables it
  // unless enable_crostini is false. The destructor resets these.
  explicit CrostiniTestHelper(TestingProfile* profile,
                              bool enable_crostini = true);
  ~CrostiniTestHelper();

  // Creates the apps named "dummy1" and "dummy2" in the default container.
  void SetupDummyApps();
  // Returns the |i|th app from the current list of apps.
  vm_tools::apps::App GetApp(int i);
  // Adds an app in the default container. Replaces an existing app with the
  // same desktop file id if one exists.
  void AddApp(const vm_tools::apps::App& app);
  // Removes the |i|th app from the current list of apps.
  void RemoveApp(int i);
  // Updates the Keywords field in an app
  void UpdateAppKeywords(
      vm_tools::apps::App& app,
      const std::map<std::string, std::set<std::string>>& keywords);

  // When the App Service is enabled, the timing of when Profile-related and
  // Profile-creation-triggered initialization occurs means that the App
  // Service has to be re-initialized with Crostini's testing fakes. This
  // method does that re-initialization.
  void ReInitializeAppServiceIntegration();

  // Set/unset the the CrostiniEnabled pref
  static void EnableCrostini(TestingProfile* profile);
  static void DisableCrostini(TestingProfile* profile);

  // Returns the app id that the registry would use for the given desktop file.
  static std::string GenerateAppId(
      const std::string& desktop_file_id,
      const std::string& vm_name = kCrostiniDefaultVmName,
      const std::string& container_name = kCrostiniDefaultContainerName);

  // Returns an App with the desktop file id, default name, and no_display
  // as provided.
  static vm_tools::apps::App BasicApp(const std::string& desktop_file_id,
                                      const std::string& name = "",
                                      bool no_display = false);

  // Returns an ApplicationList with a single desktop file.
  static vm_tools::apps::ApplicationList BasicAppList(
      const std::string& desktop_file_id,
      const std::string& vm_name,
      const std::string& container_name);

 private:
  void UpdateRegistry();

  TestingProfile* profile_;
  bool initialized_dbus_;
  vm_tools::apps::ApplicationList current_apps_;

  // This are used to allow Crostini.
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_TEST_HELPER_H_
