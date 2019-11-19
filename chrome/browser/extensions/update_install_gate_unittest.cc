// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/update_install_gate.h"

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "components/user_manager/scoped_user_manager.h"
#endif

namespace extensions {

namespace {

const char kAppId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char kPersistentExtensionId[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
const char kNonPersistentExtensionId[] = "cccccccccccccccccccccccccccccccc";

std::unique_ptr<KeyedService> BuildEventRouter(
    content::BrowserContext* profile) {
  return std::make_unique<extensions::EventRouter>(profile, nullptr);
}

scoped_refptr<const Extension> CreateApp(const std::string& extension_id,
                                         const std::string& version) {
  scoped_refptr<const Extension> app =
      ExtensionBuilder()
          .SetManifest(
              DictionaryBuilder()
                  .Set("name", "Test app")
                  .Set("version", version)
                  .Set("manifest_version", 2)
                  .Set("app",
                       DictionaryBuilder()
                           .Set("background",
                                DictionaryBuilder()
                                    .Set("scripts", ListBuilder()
                                                        .Append("background.js")
                                                        .Build())
                                    .Build())
                           .Build())
                  .Build())
          .SetID(extension_id)
          .Build();
  return app;
}

scoped_refptr<const Extension> CreateExtension(const std::string& extension_id,
                                               const std::string& version,
                                               bool persistent) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(
              DictionaryBuilder()
                  .Set("name", "Test extension")
                  .Set("version", version)
                  .Set("manifest_version", 2)
                  .Set("background", DictionaryBuilder()
                                         .Set("page", "background.html")
                                         .Set("persistent", persistent)
                                         .Build())
                  .Build())
          .SetID(extension_id)
          .Build();
  return extension;
}

ExtensionHost* CreateHost(Profile* profile, const Extension* app) {
  ProcessManager::Get(profile)->CreateBackgroundHost(
      app, BackgroundInfo::GetBackgroundURL(app));
  base::RunLoop().RunUntilIdle();

  return ProcessManager::Get(profile)->GetBackgroundHostForExtension(app->id());
}

}  // namespace

class UpdateInstallGateTest : public testing::Test {
 public:
  UpdateInstallGateTest() {
    profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
  }

  // testing::Test
  void SetUp() override {
    // Must be called from ::testing::Test::SetUp.
    ASSERT_TRUE(profile_manager_->SetUp());

    const char kUserProfile[] = "profile1@example.com";
#if defined(OS_CHROMEOS)
    const AccountId account_id(AccountId::FromUserEmail(kUserProfile));
    // Needed to allow ChromeProcessManagerDelegate to allow background pages.
    fake_user_manager_ = new chromeos::FakeChromeUserManager();
    // Takes ownership of fake_user_manager_.
    scoped_user_manager_enabler_ =
        std::make_unique<user_manager::ScopedUserManager>(
            base::WrapUnique(fake_user_manager_));
    fake_user_manager_->AddUser(account_id);
    fake_user_manager_->LoginUser(account_id);
#endif
    profile_ = profile_manager_->CreateTestingProfile(kUserProfile);
    base::RunLoop().RunUntilIdle();

    TestExtensionSystem* test_extension_system =
        static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile_));
    service_ = test_extension_system->CreateExtensionService(
        base::CommandLine::ForCurrentProcess(),
        base::FilePath() /* install_directory */,
        false /* autoupdate_enabled */);
    registry_ = ExtensionRegistry::Get(profile_);

    event_router_ = static_cast<EventRouter*>(
        EventRouterFactory::GetInstance()->SetTestingFactoryAndUse(
            profile_, base::BindRepeating(&BuildEventRouter)));

    delayer_.reset(new UpdateInstallGate(service_));

    new_app_ = CreateApp(kAppId, "2.0");
    new_persistent_ = CreateExtension(kPersistentExtensionId, "2.0", true);
    new_none_persistent_ =
        CreateExtension(kNonPersistentExtensionId, "2.0", false);
  }

  void TearDown() override { profile_manager_->DeleteAllTestingProfiles(); }

  void AddExistingExtensions() {
    scoped_refptr<const Extension> app = CreateApp(kAppId, "1.0");
    registry_->AddEnabled(app);

    scoped_refptr<const Extension> persistent =
        CreateExtension(kPersistentExtensionId, "1.0", true);
    registry_->AddEnabled(persistent);

    scoped_refptr<const Extension> none_persistent =
        CreateExtension(kNonPersistentExtensionId, "1.0", false);
    registry_->AddEnabled(none_persistent);
  }

  void MakeExtensionInUse(const std::string& extension_id) {
    const Extension* const extension =
        registry_->GetInstalledExtension(extension_id);
    ASSERT_TRUE(!!extension);
    ASSERT_TRUE(!!CreateHost(profile_, extension));
  }

  void MakeExtensionListenForOnUpdateAvailable(
      const std::string& extension_id) {
    const char kOnUpdateAvailableEvent[] = "runtime.onUpdateAvailable";
    event_router_->AddEventListener(kOnUpdateAvailableEvent, NULL,
                                    extension_id);
  }

  void Check(const Extension* extension,
             bool is_in_use,
             bool has_listener,
             bool install_immediately,
             InstallGate::Action expected_action) {
    if (is_in_use)
      MakeExtensionInUse(extension->id());
    if (has_listener)
      MakeExtensionListenForOnUpdateAvailable(extension->id());

    EXPECT_EQ(expected_action,
              delayer()->ShouldDelay(extension, install_immediately));
  }

  UpdateInstallGate* delayer() { return delayer_.get(); }
  ExtensionService* service() { return service_; }

  const Extension* new_app() const { return new_app_.get(); }
  const Extension* new_persistent() const { return new_persistent_.get(); }
  const Extension* new_none_persistent() const {
    return new_none_persistent_.get();
  }

 private:
  // Needed by extension system.
  content::BrowserTaskEnvironment task_environment_;

  // Needed to ensure we don't end up creating actual RenderViewHosts
  // and RenderProcessHosts.
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;

  TestingProfile* profile_ = nullptr;
  std::unique_ptr<TestingProfileManager> profile_manager_;

  ExtensionService* service_ = nullptr;
  ExtensionRegistry* registry_ = nullptr;
  EventRouter* event_router_ = nullptr;

#if defined(OS_CHROMEOS)
  // Needed for creating ExtensionService.
  chromeos::FakeChromeUserManager* fake_user_manager_ = nullptr;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_enabler_;
#endif

  std::unique_ptr<UpdateInstallGate> delayer_;

  scoped_refptr<const Extension> new_app_;
  scoped_refptr<const Extension> new_persistent_;
  scoped_refptr<const Extension> new_none_persistent_;

  DISALLOW_COPY_AND_ASSIGN(UpdateInstallGateTest);
};

TEST_F(UpdateInstallGateTest, InstallOnServiceNotReady) {
  ASSERT_FALSE(service()->is_ready());
  Check(new_app(), false, false, false, InstallGate::INSTALL);
  Check(new_persistent(), false, false, false, InstallGate::INSTALL);
  Check(new_none_persistent(), false, false, false, InstallGate::INSTALL);
}

TEST_F(UpdateInstallGateTest, InstallOnFirstInstall) {
  service()->Init();
  Check(new_app(), false, false, false, InstallGate::INSTALL);
  Check(new_persistent(), false, false, false, InstallGate::INSTALL);
  Check(new_none_persistent(), false, false, false, InstallGate::INSTALL);
}

TEST_F(UpdateInstallGateTest, InstallOnInstallImmediately) {
  service()->Init();
  AddExistingExtensions();

  const bool kInstallImmediately = true;
  for (bool in_use : {false, true}) {
    for (bool has_listener : {false, true}) {
      Check(new_app(), in_use, has_listener, kInstallImmediately,
            InstallGate::INSTALL);
      Check(new_persistent(), in_use, has_listener, kInstallImmediately,
            InstallGate::INSTALL);
      Check(new_none_persistent(), in_use, has_listener, kInstallImmediately,
            InstallGate::INSTALL);
    }
  }
}

TEST_F(UpdateInstallGateTest, DelayInstallWhenInUse) {
  service()->Init();
  AddExistingExtensions();

  const bool kInUse = true;
  const bool kDontInstallImmediately = false;
  for (bool has_listener : {false, true}) {
    Check(new_app(), kInUse, has_listener, kDontInstallImmediately,
          InstallGate::DELAY);
    Check(new_persistent(), kInUse, has_listener, kDontInstallImmediately,
          has_listener ? InstallGate::DELAY : InstallGate::INSTALL);
    Check(new_none_persistent(), kInUse, has_listener, kDontInstallImmediately,
          InstallGate::DELAY);
  }
}

}  // namespace extensions
