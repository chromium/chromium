// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/update_install_gate.h"

#include <memory>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/scoped_test_mv2_enabler.h"
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

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/login/test/chrome_user_session_test_environment_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "google_apis/gaia/gaia_id.h"
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
              base::DictValue()
                  .Set("name", "Test app")
                  .Set("version", version)
                  .Set("manifest_version", 2)
                  .Set("app", base::DictValue().Set(
                                  "background",
                                  base::DictValue().Set(
                                      "scripts", base::ListValue().Append(
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
              base::DictValue()
                  .Set("name", "Test extension")
                  .Set("version", version)
                  .Set("manifest_version", 2)
                  .Set("background", base::DictValue()
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
  UpdateInstallGateTest() = default;

  UpdateInstallGateTest(const UpdateInstallGateTest&) = delete;
  UpdateInstallGateTest& operator=(const UpdateInstallGateTest&) = delete;

  // testing::Test
  void SetUp() override {
    // Must be called from ::testing::Test::SetUp.
#if BUILDFLAG(IS_CHROMEOS)
    // Needed to allow ChromeProcessManagerDelegate to allow background pages.
    const AccountId account_id = AccountId::FromUserEmailGaiaId(
        "profile1@example.com", GaiaId("1234567890"));

    // The session environment owns the ProfileManager and creates the signed
    // in user's profile, so that the production code under test sees a real
    // UserManager and SessionManager.
    TestingBrowserProcess* const browser_process =
        TestingBrowserProcess::GetGlobal();
    user_session_test_environment_ = std::make_unique<
        ash::test::UserSessionTestEnvironment>(
        browser_process->local_state(),
        std::make_unique<ash::test::ChromeUserSessionTestEnvironmentDelegate>(
            browser_process));
    ASSERT_TRUE(user_session_test_environment_->AddRegularUser(account_id));
    user_session_test_environment_->LogIn(account_id);
    profile_ = Profile::FromBrowserContext(
        ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
            account_id));
    ASSERT_TRUE(profile_);
#else
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("profile1@example.com");
#endif
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
    delayer_.reset();
    event_router_ = nullptr;
    registry_ = nullptr;
    service_ = nullptr;
    system_ = nullptr;
    profile_ = nullptr;
#if BUILDFLAG(IS_CHROMEOS)
    user_session_test_environment_.reset();
#else
    profile_manager_->DeleteAllTestingProfiles();
#endif
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

#if BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<ash::test::UserSessionTestEnvironment>
      user_session_test_environment_;
#else
  std::unique_ptr<TestingProfileManager> profile_manager_;
#endif

  raw_ptr<Profile> profile_ = nullptr;
  std::unique_ptr<content::RenderProcessHost> render_process_host_;

  raw_ptr<TestExtensionSystem> system_ = nullptr;
  raw_ptr<ExtensionService> service_ = nullptr;
  raw_ptr<ExtensionRegistry> registry_ = nullptr;
  raw_ptr<EventRouter> event_router_ = nullptr;

  std::unique_ptr<UpdateInstallGate> delayer_;

  scoped_refptr<const Extension> new_app_;
  scoped_refptr<const Extension> new_persistent_;
  scoped_refptr<const Extension> new_none_persistent_;

  // This test relies on legacy MV2 extensions with persistent and non-
  // persistent background pages.
  // TODO(https://crbug.com/40804030): Migrate this to only rely on MV3
  // extensions.
  ScopedTestMV2Enabler mv2_enabler_;
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
