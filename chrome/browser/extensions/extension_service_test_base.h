// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SERVICE_TEST_BASE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SERVICE_TEST_BASE_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/at_exit.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/sandboxed_unpacker.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/scoped_test_user_manager.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#endif

class Profile;
class TestingProfile;

namespace content {
class BrowserContext;
}

namespace sync_preferences {
class TestingPrefServiceSyncable;
}

namespace extensions {

class ExtensionRegistry;
class ExtensionService;

// A unittest infrastructure which creates an ExtensionService. Whenever
// possible, use this instead of creating a browsertest.
// Note: Before adding methods to this class, please, please, please think about
// whether they should go here or in a more specific subclass. Lots of things
// need an ExtensionService, but they don't all need to know how you want yours
// to be initialized.
class ExtensionServiceTestBase : public testing::Test {
 public:
  struct ExtensionServiceInitParams {
    base::FilePath profile_path;
    base::FilePath pref_file;
    base::FilePath extensions_install_dir;
    bool autoupdate_enabled = false;
    bool extensions_enabled = true;
    bool is_first_run = true;
    bool profile_is_supervised = false;

    // Though you could use this constructor, you probably want to use
    // CreateDefaultInitParams(), and then make a change or two.
    ExtensionServiceInitParams();
    ExtensionServiceInitParams(const ExtensionServiceInitParams& other);
  };

  // Public because parameterized test cases need it to be, or else the compiler
  // barfs.
  static void SetUpTestCase();  // faux-verride (static override).

 protected:
  ExtensionServiceTestBase();
  ~ExtensionServiceTestBase() override;

  // testing::Test implementation.
  void SetUp() override;
  void TearDown() override;

  // Create a set of InitParams to install an ExtensionService into |temp_dir_|.
  ExtensionServiceInitParams CreateDefaultInitParams();

  // Initialize an ExtensionService according to the given |params|.
  virtual void InitializeExtensionService(
      const ExtensionServiceInitParams& params);

  // Initialize an empty ExtensionService using the default init params.
  void InitializeEmptyExtensionService();

  // Initialize an ExtensionService with the associated |prefs_file| and
  // |source_install_dir|.
  void InitializeInstalledExtensionService(
      const base::FilePath& prefs_file,
      const base::FilePath& source_install_dir);

  // Initialize an ExtensionService with a few already-installed extensions.
  void InitializeGoodInstalledExtensionService();

  // Initialize an ExtensionService with autoupdate enabled.
  void InitializeExtensionServiceWithUpdater();

  // Initializes an ExtensionService without extensions enabled.
  void InitializeExtensionServiceWithExtensionsDisabled();

  // Helpers to check the existence and values of extension prefs.
  size_t GetPrefKeyCount();
  void ValidatePrefKeyCount(size_t count);
  testing::AssertionResult ValidateBooleanPref(
      const std::string& extension_id,
      const std::string& pref_path,
      bool expected_val);
  void ValidateIntegerPref(const std::string& extension_id,
                           const std::string& pref_path,
                           int expected_val);
  void ValidateStringPref(const std::string& extension_id,
                          const std::string& pref_path,
                          const std::string& expected_val);

  // TODO(rdevlin.cronin): Pull out more methods from ExtensionServiceTest that
  // are commonly used and/or reimplemented. For instance, methods to install
  // extensions from various locations, etc.

  content::BrowserContext* browser_context();
  Profile* profile();
  TestingProfile* testing_profile() { return profile_.get(); }
  sync_preferences::TestingPrefServiceSyncable* testing_pref_service();
  ExtensionService* service() { return service_; }
  ExtensionRegistry* registry() { return registry_; }
  const base::FilePath& extensions_install_dir() const {
    return extensions_install_dir_;
  }
  const base::FilePath& data_dir() const { return data_dir_; }
  const base::ScopedTempDir& temp_dir() const { return temp_dir_; }

 private:
  // Must be declared before anything that may make use of the
  // directory so as to ensure files are closed before cleanup.
  base::ScopedTempDir temp_dir_;

  // Destroying at_exit_manager_ will delete all LazyInstances, so it must come
  // after task_environment_ in the destruction order.
  base::ShadowingAtExitManager at_exit_manager_;

  // The MessageLoop is used by RenderViewHostTestEnabler, so this must be
  // created before it.
  content::BrowserTaskEnvironment task_environment_;

  // Enable creation of WebContents without initializing a renderer.
  content::RenderViewHostTestEnabler rvh_test_enabler_;

 protected:
  // It's unfortunate that these are exposed to subclasses (rather than used
  // through the accessor methods above), but too many tests already use them
  // directly.

  // The associated testing profile.
  std::unique_ptr<TestingProfile> profile_;

  // The ExtensionService, whose lifetime is managed by |profile|'s
  // ExtensionSystem.
  ExtensionService* service_;
  ScopedTestingLocalState testing_local_state_;

 private:
  void CreateExtensionService(const ExtensionServiceInitParams& params);

  // The directory into which extensions are installed.
  base::FilePath extensions_install_dir_;

  // chrome/test/data/extensions/
  base::FilePath data_dir_;

  content::InProcessUtilityThreadHelper in_process_utility_thread_helper_;

  // The associated ExtensionRegistry, for convenience.
  extensions::ExtensionRegistry* registry_;

#if defined OS_CHROMEOS
  chromeos::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  chromeos::ScopedTestUserManager test_user_manager_;
#endif

  // An override that ignores CRX3 publisher signatures.
  SandboxedUnpacker::ScopedVerifierFormatOverrideForTest
      verifier_format_override_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionServiceTestBase);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SERVICE_TEST_BASE_H_
