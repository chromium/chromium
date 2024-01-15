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
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/browser/extensions/test_extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::Extension;

using testing::_;
using testing::Invoke;
using testing::WithArg;

constexpr char kExemptExtensionId[] = "abcdefghijklmnopabcdefghijklmnop";
constexpr char kExtensionId1[] = "bcdefghijklmnopabcdefghijklmnopa";
constexpr char kExtensionId2[] = "cdefghijklmnopabcdefghijklmnopab";

namespace chromeos {

namespace {

// TODO(mpetrisor, b:202288170) Fix ExtensionService mock.
class MockExtensionService : public extensions::ExtensionService {
 public:
  MockExtensionService(Profile* profile,
                       const base::CommandLine* command_line,
                       const base::FilePath& install_directory,
                       const base::FilePath& unpacked_install_directory,
                       extensions::ExtensionPrefs* extension_prefs,
                       extensions::Blocklist* blocklist,
                       bool autoupdate_enabled,
                       bool extensions_enabled,
                       base::OneShotEvent* ready)
      : extensions::ExtensionService(profile,
                                     command_line,
                                     install_directory,
                                     unpacked_install_directory,
                                     extension_prefs,
                                     blocklist,
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
  ExtensionCleanupHandlerUnittest()
      : mock_profile_manager_(TestingBrowserProcess::GetGlobal()) {
    auto fake_user_manager = std::make_unique<FakeChromeUserManager>();
    fake_user_manager_ = fake_user_manager.get();
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));
  }

  void SetUp() override {
    ASSERT_TRUE(mock_profile_manager_.SetUp());

    // Add a user.
    const AccountId test_account_id(
        AccountId::FromUserEmail("test-user@example.com"));
    fake_user_manager_->AddUser(test_account_id);
    fake_user_manager_->LoginUser(test_account_id);

    // Create a valid profile for the user.
    mock_profile_ = mock_profile_manager_.CreateTestingProfile(
        test_account_id.GetUserEmail());
    EXPECT_EQ(ProfileManager::GetActiveUserProfile(), mock_profile_);
    mock_prefs_ = mock_profile_->GetTestingPrefService();
    extension_registry_ = extensions::ExtensionRegistry::Get(mock_profile_);

    // Set up extension service.
    extensions::TestExtensionSystem* extension_system =
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(mock_profile_));
    extension_system->CreateExtensionService(
        base::CommandLine::ForCurrentProcess(), base::FilePath(), false);
    extension_service_ = static_cast<MockExtensionService*>(
        extensions::ExtensionSystem::Get(mock_profile_)->extension_service());

    extension_cleanup_handler_ = std::make_unique<ExtensionCleanupHandler>();
  }

  void TearDown() override {
    extension_cleanup_handler_.reset();
    extensions::ExtensionSystem::Get(mock_profile_)->Shutdown();
    testing::Test::TearDown();
  }

  void SetupExemptList() {
    mock_prefs_->SetManagedPref(
        prefs::kRestrictedManagedGuestSessionExtensionCleanupExemptList,
        base::Value::List().Append(kExemptExtensionId));
  }

  content::BrowserTaskEnvironment task_environment_;
  raw_ptr<sync_preferences::TestingPrefServiceSyncable, DanglingUntriaged>
      mock_prefs_;
  TestingProfileManager mock_profile_manager_;
  raw_ptr<FakeChromeUserManager, DanglingUntriaged> fake_user_manager_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
  std::unique_ptr<ExtensionCleanupHandler> extension_cleanup_handler_;
  raw_ptr<MockExtensionService> extension_service_;
  raw_ptr<extensions::ExtensionRegistry> extension_registry_;
  raw_ptr<TestingProfile> mock_profile_;
};

scoped_refptr<const Extension> MakeExtensionNamed(const std::string& name,
                                                  const std::string& id) {
  return extensions::ExtensionBuilder(name).SetID(id).Build();
}

TEST_F(ExtensionCleanupHandlerUnittest, Cleanup) {
  extensions::ExtensionSystem::Get(mock_profile_)
      ->extension_service()
      ->AddExtension(MakeExtensionNamed("foo", kExemptExtensionId).get());
  extensions::ExtensionSystem::Get(mock_profile_)
      ->extension_service()
      ->AddExtension(MakeExtensionNamed("bar", kExtensionId1).get());
  extensions::ExtensionSystem::Get(mock_profile_)
      ->extension_service()
      ->AddExtension(MakeExtensionNamed("baz", kExtensionId2).get());

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
