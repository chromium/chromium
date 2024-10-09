// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SERVICE_TEST_BASE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SERVICE_TEST_BASE_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/scoped_test_mv2_enabler.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/sandboxed_unpacker.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "components/user_manager/scoped_user_manager.h"
#endif

class Profile;
class TestingProfile;

namespace content {
class BrowserContext;
class BrowserTaskEnvironment;
}  // namespace content

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
    // If set, even if it is empty string, creates a pref file in the profile
    // directory with the given content, and initializes user prefs store
    // referring the file.
    // If not, sync_preferences::TestingPrefServiceSyncable is used.
    std::optional<std::string> prefs_content;

    // If not empty, copies both directories to the profile directory.
    base::FilePath extensions_dir;
    base::FilePath unpacked_extensions_dir;

    bool autoupdate_enabled = false;
    bool extensions_enabled = true;
    bool is_first_run = true;
    bool profile_is_supervised = false;
    bool profile_is_guest = false;
    bool enable_bookmark_model = false;
    bool enable_install_limiter = false;

    TestingProfile::TestingFactories testing_factories;

    ExtensionServiceInitParams();
    ExtensionServiceInitParams(ExtensionServiceInitParams&& other);
    ExtensionServiceInitParams& operator=(ExtensionServiceInitParams&& other) =
        delete;
    ExtensionServiceInitParams(const ExtensionServiceInitParams& other) =
        delete;
    ExtensionServiceInitParams& operator=(
        const ExtensionServiceInitParams& other) = delete;
    ~ExtensionServiceInitParams();

    // Sets the prefs_content to the content in the given file.
    [[nodiscard]] bool SetPrefsContentFromFile(const base::FilePath& filepath);

    // Configures prefs_content and extensions_dir from the test data directory
    // specified by the `filepath`.
    // There must be a file named "Preferences" in the test data directory
    // containing the prefs content.
    // Also, there must be a directory named "Extensions" containing extensions
    // data for testing.
    [[nodiscard]] bool ConfigureByTestDataDirectory(
        const base::FilePath& filepath);
  };

  ExtensionServiceTestBase(const ExtensionServiceTestBase&) = delete;
  ExtensionServiceTestBase& operator=(const ExtensionServiceTestBase&) = delete;

  // Public because parameterized test cases need it to be, or else the compiler
  // barfs.
  static void SetUpTestSuite();  // faux-verride (static override).

 protected:
  ExtensionServiceTestBase();
  // Alternatively, a subclass may pass a BrowserTaskEnvironment directly.
  explicit ExtensionServiceTestBase(
      std::unique_ptr<content::BrowserTaskEnvironment> task_environment);

  ~ExtensionServiceTestBase() override;

  // testing::Test implementation.
  void SetUp() override;
  void TearDown() override;

  // Initialize an ExtensionService according to the given |params|.
  virtual void InitializeExtensionService(ExtensionServiceInitParams params);

  // Whether MV2 extensions should be allowed. Defaults to true.
  virtual bool ShouldAllowMV2Extensions();

  // Initialize an empty ExtensionService using a production, on-disk pref file.
  // See documentation for |prefs_content|.
  void InitializeEmptyExtensionService();

  // Initialize an ExtensionService with a few already-installed extensions.
  void InitializeGoodInstalledExtensionService();

  // Initialize an ExtensionService with autoupdate enabled.
  void InitializeExtensionServiceWithUpdater();

  // Initializes an ExtensionService without extensions enabled.
  void InitializeExtensionServiceWithExtensionsDisabled();

  // Helpers to check the existence and values of extension prefs.
  size_t GetPrefKeyCount();
  void ValidatePrefKeyCount(size_t count);
  testing::AssertionResult ValidateBooleanPref(const std::string& extension_id,
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
  const base::FilePath& unpacked_install_dir() const {
    return unpacked_install_dir_;
  }
  const base::FilePath& data_dir() const { return data_dir_; }
  const base::ScopedTempDir& temp_dir() const { return temp_dir_; }
  content::BrowserTaskEnvironment* task_environment() {
    return task_environment_.get();
  }
  policy::MockConfigurationPolicyProvider* policy_provider() {
    return &policy_provider_;
  }
  policy::PolicyService* policy_service() { return policy_service_.get(); }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::ScopedCrosSettingsTestHelper& cros_settings_test_helper() {
    return cros_settings_test_helper_;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // If a test uses a feature list, it should be destroyed after
  // |task_environment_|, to avoid tsan data races between the ScopedFeatureList
  // destructor, and any tasks running on different threads that check if a
  // feature is enabled. ~BrowserTaskEnvironment will make sure those tasks
  // finish before |feature_list_| is destroyed.
  base::test::ScopedFeatureList feature_list_;

 private:
  // Must be declared before anything that may make use of the
  // directory so as to ensure files are closed before cleanup.
  base::ScopedTempDir temp_dir_;

  // The MessageLoop is used by RenderViewHostTestEnabler, so this must be
  // created before it.
  std::unique_ptr<content::BrowserTaskEnvironment> task_environment_;

  // Enable creation of WebContents without initializing a renderer.
  content::RenderViewHostTestEnabler rvh_test_enabler_;

  // Provides policies for the PolicyService below, so this must be created
  // before it.
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;

  // PolicyService for the testing profile, so unit tests can use custom
  // policies.
  std::unique_ptr<policy::PolicyService> policy_service_;

 protected:
  // It's unfortunate that these are exposed to subclasses (rather than used
  // through the accessor methods above), but too many tests already use them
  // directly.

  // The associated testing profile.
  std::unique_ptr<TestingProfile> profile_;

  // The ExtensionService, whose lifetime is managed by |profile|'s
  // ExtensionSystem.
  raw_ptr<ExtensionService, DanglingUntriaged> service_;
  ScopedTestingLocalState testing_local_state_;

 private:
  void CreateExtensionService(bool is_first_run,
                              bool autoupdate_enabled,
                              bool extensions_enabled,
                              bool enable_install_limiter);

  // The directory into which extensions are installed.
  base::FilePath extensions_install_dir_;
  // The directory into which unpacked extensions are installed.
  base::FilePath unpacked_install_dir_;

  // chrome/test/data/extensions/
  base::FilePath data_dir_;

  content::InProcessUtilityThreadHelper in_process_utility_thread_helper_;

  // The associated ExtensionRegistry, for convenience.
  raw_ptr<extensions::ExtensionRegistry, DanglingUntriaged> registry_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  std::unique_ptr<ash::KioskChromeAppManager> kiosk_chrome_app_manager_;
  user_manager::ScopedUserManager user_manager_;
#endif

  // An override that ignores CRX3 publisher signatures.
  SandboxedUnpacker::ScopedVerifierFormatOverrideForTest
      verifier_format_override_;

  // An override that allows MV2 extensions to be loaded.
  std::optional<ScopedTestMV2Enabler> mv2_enabler_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SERVICE_TEST_BASE_H_
