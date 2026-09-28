// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/extension_cleanup_handler.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/login/test/chrome_user_session_test_environment_delegate.h"
#include "chrome/browser/extensions/extension_error_controller.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/browser/extensions/test_extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/account_id/account_id.h"
#include "components/account_id/account_id_literal.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension_builder.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::Extension;

using testing::_;
using testing::WithArg;

constexpr char kExemptExtensionId[] = "abcdefghijklmnopabcdefghijklmnop";
constexpr char kExtensionId1[] = "bcdefghijklmnopabcdefghijklmnopa";
constexpr char kExtensionId2[] = "cdefghijklmnopabcdefghijklmnopab";

namespace chromeos {

namespace {

constexpr AccountId::Literal kTestAccountId =
    AccountId::Literal::FromUserEmailGaiaId("test-user@example.com",
                                            GaiaId::Literal("1234567890"));

// TODO(mpetrisor, b:202288170) Fix ExtensionService mock.
class MockExtensionService : public extensions::ExtensionService {
 public:
  MockExtensionService(Profile* profile,
                       const base::CommandLine* command_line,
                       const base::FilePath& install_directory,
                       const base::FilePath& unpacked_install_directory,
                       extensions::ExtensionPrefs* extension_prefs,
                       extensions::Blocklist* blocklist,
                       extensions::ExtensionErrorController* error_controller,
                       bool autoupdate_enabled,
                       bool extensions_enabled,
                       base::OneShotEvent* ready)
      : extensions::ExtensionService(profile,
                                     command_line,
                                     install_directory,
                                     unpacked_install_directory,
                                     extension_prefs,
                                     blocklist,
                                     error_controller,
                                     autoupdate_enabled,
                                     extensions_enabled,
                                     ready) {}

  MockExtensionService(const MockExtensionService&) = delete;
  MockExtensionService& operator=(const MockExtensionService&) = delete;

  ~MockExtensionService() override = default;

  MOCK_METHOD(bool,
              UninstallExtension,
              (const std::string& extension_id,
               extensions::UninstallReason reason,
               std::u16string* error,
               base::OnceClosure callback));
};

}  // namespace

class ExtensionCleanupHandlerUnittest : public testing::Test {
 protected:
  void SetUp() override {
    auto* browser_process = TestingBrowserProcess::GetGlobal();
    user_session_test_environment_ = std::make_unique<
        ash::test::UserSessionTestEnvironment>(
        browser_process->local_state(),
        std::make_unique<ash::test::ChromeUserSessionTestEnvironmentDelegate>(
            browser_process));
    ASSERT_TRUE(user_session_test_environment_->AddRegularUser(kTestAccountId));
    user_session_test_environment_->LogIn(kTestAccountId);

    profile_ = static_cast<TestingProfile*>(Profile::FromBrowserContext(
        ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
            kTestAccountId)));
    ASSERT_TRUE(profile_);
    EXPECT_EQ(ProfileManager::GetActiveUserProfile(), profile_);
    prefs_ = profile_->GetTestingPrefService();
    extension_registry_ = extensions::ExtensionRegistry::Get(profile_);

    // Set up extension service.
    extensions::TestExtensionSystem* extension_system =
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile_));
    extension_system->CreateExtensionService(
        base::CommandLine::ForCurrentProcess(), base::FilePath(), false);
    extension_service_ = static_cast<MockExtensionService*>(
        extensions::ExtensionSystem::Get(profile_)->extension_service());

    extension_cleanup_handler_ = std::make_unique<ExtensionCleanupHandler>();
  }

  void TearDown() override {
    extension_cleanup_handler_.reset();
    extension_service_ = nullptr;
    extension_registry_ = nullptr;
    prefs_ = nullptr;
    profile_ = nullptr;
    user_session_test_environment_.reset();
    testing::Test::TearDown();
  }

  void SetupExemptList() {
    prefs_->SetManagedPref(
        prefs::kRestrictedManagedGuestSessionExtensionCleanupExemptList,
        base::ListValue().Append(kExemptExtensionId));
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<ash::test::UserSessionTestEnvironment>
      user_session_test_environment_;
  raw_ptr<TestingProfile> profile_;
  raw_ptr<sync_preferences::TestingPrefServiceSyncable> prefs_;
  raw_ptr<extensions::ExtensionRegistry> extension_registry_;
  raw_ptr<MockExtensionService> extension_service_;
  std::unique_ptr<ExtensionCleanupHandler> extension_cleanup_handler_;
};

scoped_refptr<const Extension> MakeExtensionNamed(const std::string& name,
                                                  const std::string& id) {
  return extensions::ExtensionBuilder(name).SetID(id).Build();
}

TEST_F(ExtensionCleanupHandlerUnittest, Cleanup) {
  extensions::ExtensionRegistrar::Get(profile_)->AddExtension(
      MakeExtensionNamed("foo", kExemptExtensionId));
  extensions::ExtensionRegistrar::Get(profile_)->AddExtension(
      MakeExtensionNamed("bar", kExtensionId1));
  extensions::ExtensionRegistrar::Get(profile_)->AddExtension(
      MakeExtensionNamed("baz", kExtensionId2));

  SetupExemptList();
  extensions::ExtensionSet all_installed_extensions =
      extension_registry_->GenerateInstalledExtensionsSet();
  EXPECT_EQ(all_installed_extensions.size(), 3u);

  base::RunLoop run_loop;
  extension_cleanup_handler_->Cleanup(
      base::BindLambdaForTesting([&](const std::optional<std::string>& error) {
        EXPECT_EQ(error, std::nullopt);
        run_loop.QuitClosure().Run();
      }));
  run_loop.Run();

  all_installed_extensions =
      extension_registry_->GenerateInstalledExtensionsSet();
  EXPECT_EQ(all_installed_extensions.size(), 1u);
  EXPECT_TRUE(all_installed_extensions.Contains(kExemptExtensionId));
}

}  // namespace chromeos
