// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/update_install_gate.h"

#include <memory>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_renderer_host.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#endif

namespace extensions {

namespace {

const char kAppId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char kPersistentExtensionId[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
const char kNonPersistentExtensionId[] = "cccccccccccccccccccccccccccccccc";

std::unique_ptr<KeyedService> BuildEventRouter(
    content::BrowserContext* profile) {
  return std::make_unique<EventRouter>(profile, nullptr);
}

scoped_refptr<const Extension> CreateApp(const std::string& extension_id,
                                         const std::string& version) {
  scoped_refptr<const Extension> app =
      ExtensionBuilder()
          .SetManifest(
              base::Value::Dict()
                  .Set("name", "Test app")
                  .Set("version", version)
                  .Set("manifest_version", 2)
                  .Set("app", base::Value::Dict().Set(
                                  "background",
                                  base::Value::Dict().Set(
                                      "scripts", base::Value::List().Append(
                                                     "background.js")))))
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
              base::Value::Dict()
                  .Set("name", "Test extension")
                  .Set("version", version)
                  .Set("manifest_version", 2)
                  .Set("background", base::Value::Dict()
                                         .Set("page", "background.html")
                                         .Set("persistent", persistent)))
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
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
  }

  UpdateInstallGateTest(const UpdateInstallGateTest&) = delete;
  UpdateInstallGateTest& operator=(const UpdateInstallGateTest&) = delete;

  // testing::Test
  void SetUp() override {
    // Must be called from ::testing::Test::SetUp.
    ASSERT_TRUE(profile_manager_->SetUp());

    const char kUserProfile[] = "profile1@example.com";
#if BUILDFLAG(IS_CHROMEOS_ASH)
    const AccountId account_id(AccountId::FromUserEmail(kUserProfile));
    // Needed to allow ChromeProcessManagerDelegate to allow background pages.
    fake_user_manager_ = new ash::FakeChromeUserManager();
    // Takes ownership of fake_user_manager_.
    scoped_user_manager_enabler_ =
        std::make_unique<user_manager::ScopedUserManager>(
            base::WrapUnique(fake_user_manager_.get()));
    fake_user_manager_->AddUser(account_id);
    fake_user_manager_->LoginUser(account_id);
#endif
    profile_ = profile_manager_->CreateTestingProfile(kUserProfile);
    render_process_host_ =
        std::make_unique<content::MockRenderProcessHost>(profile_);
    base::RunLoop().RunUntilIdle();

    system_ = static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile_));
    service_ = system_->CreateExtensionService(
        base::CommandLine::ForCurrentProcess(),
        base::FilePath() /* install_directory */,
        false /* autoupdate_enabled */);
    registry_ = ExtensionRegistry::Get(profile_);

    event_router_ = static_cast<EventRouter*>(
        EventRouterFactory::GetInstance()->SetTestingFactoryAndUse(
            profile_, base::BindRepeating(&BuildEventRouter)));

    delayer_ = std::make_unique<UpdateInstallGate>(profile_);

    new_app_ = CreateApp(kAppId, "2.0");
    new_persistent_ = CreateExtension(kPersistentExtensionId, "2.0", true);
    new_none_persistent_ =
        CreateExtension(kNonPersistentExtensionId, "2.0", false);
  }

  void TearDown() override {
    render_process_host_.reset();
    profile_manager_->DeleteAllTestingProfiles();
  }

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
    ASSERT_TRUE(extension);
    ASSERT_TRUE(CreateHost(profile_, extension));
  }

  void MakeExtensionListenForOnUpdateAvailable(
      const std::string& extension_id) {
    const char kOnUpdateAvailableEvent[] = "runtime.onUpdateAvailable";

    event_router_->AddEventListener(kOnUpdateAvailableEvent,
                                    render_process_host(), extension_id);
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
  ExtensionSystem* system() { return system_; }
  ExtensionService* service() { return service_; }

  const Extension* new_app() const { return new_app_.get(); }
  const Extension* new_persistent() const { return new_persistent_.get(); }
  const Extension* new_none_persistent() const {
    return new_none_persistent_.get();
  }

  content::RenderProcessHost* render_process_host() const {
    return render_process_host_.get();
  }

 private:
  // Needed by extension system.
  content::BrowserTaskEnvironment task_environment_;

  // Needed to ensure we don't end up creating actual RenderViewHosts
  // and RenderProcessHosts.
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;

  raw_ptr<TestingProfile, DanglingUntriaged> profile_ = nullptr;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<content::RenderProcessHost> render_process_host_;

  raw_ptr<TestExtensionSystem, DanglingUntriaged> system_ = nullptr;
  raw_ptr<ExtensionService, DanglingUntriaged> service_ = nullptr;
  raw_ptr<ExtensionRegistry, DanglingUntriaged> registry_ = nullptr;
  raw_ptr<EventRouter, DanglingUntriaged> event_router_ = nullptr;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Needed for creating ExtensionService.
  raw_ptr<ash::FakeChromeUserManager, DanglingUntriaged> fake_user_manager_ =
      nullptr;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_enabler_;
#endif

  std::unique_ptr<UpdateInstallGate> delayer_;

  scoped_refptr<const Extension> new_app_;
  scoped_refptr<const Extension> new_persistent_;
  scoped_refptr<const Extension> new_none_persistent_;
};

TEST_F(UpdateInstallGateTest, InstallOnServiceNotReady) {
  ASSERT_FALSE(system()->is_ready());
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
