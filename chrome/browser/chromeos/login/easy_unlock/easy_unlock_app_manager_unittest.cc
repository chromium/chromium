// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_app_manager.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/chromeos/login/users/scoped_test_user_manager.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/extensions/api/screenlock_private/screenlock_private_api.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/common/apps/platform_apps/api/easy_unlock_private.h"
#include "chrome/common/extensions/api/screenlock_private.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/components/proximity_auth/switches.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_factory.h"
#include "extensions/browser/test_event_router.h"
#include "extensions/common/api/app_runtime.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace easy_unlock_private_api = chrome_apps::api::easy_unlock_private;
namespace screenlock_private_api = extensions::api::screenlock_private;
namespace app_runtime_api = extensions::api::app_runtime;

namespace chromeos {
namespace {

// Sets |*value| to true, also verifying that the value was not previously set.
// Used in tests for verifying that a callback was called.
void VerifyFalseAndSetToTrue(bool* value) {
  EXPECT_FALSE(*value);
  *value = true;
}

// A ProcessManager that doesn't create background host pages.
class TestProcessManager : public extensions::ProcessManager {
 public:
  explicit TestProcessManager(content::BrowserContext* context)
      : extensions::ProcessManager(
            context,
            context,
            extensions::ExtensionRegistry::Get(context)) {}
  ~TestProcessManager() override {}

  // ProcessManager overrides:
  bool CreateBackgroundHost(const extensions::Extension* extension,
                            const GURL& url) override {
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestProcessManager);
};

std::unique_ptr<KeyedService> CreateTestProcessManager(
    content::BrowserContext* context) {
  return std::make_unique<TestProcessManager>(context);
}

std::unique_ptr<KeyedService> CreateScreenlockPrivateEventRouter(
    content::BrowserContext* context) {
  return std::make_unique<extensions::ScreenlockPrivateEventRouter>(context);
}

// Observes extension registry for unload and load events (in that order) of an
// extension with the provided extension id.
// Used to determine if an extension was reloaded.
class ExtensionReloadTracker : public extensions::ExtensionRegistryObserver {
 public:
  ExtensionReloadTracker(Profile* profile, const std::string& extension_id)
      : profile_(profile),
        extension_id_(extension_id),
        unloaded_(false),
        loaded_(false) {
    extensions::ExtensionRegistry::Get(profile)->AddObserver(this);
  }

  ~ExtensionReloadTracker() override {
    extensions::ExtensionRegistry::Get(profile_)->RemoveObserver(this);
  }

  // extension::ExtensionRegistryObserver implementation:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override {
    ASSERT_FALSE(loaded_);
    ASSERT_EQ(extension_id_, extension->id());
    loaded_ = true;
  }

  void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      extensions::UnloadedExtensionReason reason) override {
    ASSERT_FALSE(unloaded_);
    ASSERT_EQ(extension_id_, extension->id());
    unloaded_ = true;
  }

  // Whether the extensino was unloaded and loaded during |this| lifetime.
  bool HasReloaded() const { return loaded_ && unloaded_; }

 private:
  Profile* profile_;
  std::string extension_id_;
  bool unloaded_;
  bool loaded_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionReloadTracker);
};

// Consumes events dispatched from test event router.
class EasyUnlockAppEventConsumer
    : public extensions::TestEventRouter::EventObserver {
 public:
  EasyUnlockAppEventConsumer() = default;
  ~EasyUnlockAppEventConsumer() override = default;

  // extensions::TestEventRouter::EventObserver:
  void OnDispatchEventToExtension(const std::string& extension_id,
                                  const extensions::Event& event) override {
    if (event.event_name ==
               screenlock_private_api::OnAuthAttempted::kEventName) {
      ConsumeAuthAttempted(event.event_args.get());
    } else {
      ASSERT_EQ(app_runtime_api::OnLaunched::kEventName, event.event_name)
          << "Unexpected event: " << event.event_name;
    }
  }

  void OnBroadcastEvent(const extensions::Event& event) override {
    ASSERT_EQ(screenlock_private_api::OnAuthAttempted::kEventName,
              event.event_name);
    ConsumeAuthAttempted(event.event_args.get());
  }

  // The data carried by the last UserInfoUpdated event:
  std::string user_id() const { return user_id_; }
  bool user_logged_in() const { return user_logged_in_; }
  bool user_data_ready() const { return user_data_ready_; }

 private:
  // Processes easyUnlockPrivate.onUserInfoUpdated event.
  void ConsumeUserInfoUpdated(base::ListValue* args) {
    if (!args) {
      ADD_FAILURE() << "No argument list for onUserInfoUpdated event.";
      return;
    }

    if (args->GetSize() != 1u) {
      ADD_FAILURE()
          << "Invalid argument list size for onUserInfoUpdated event: "
          << args->GetSize() << " expected: " << 1u;
      return;
    }

    base::DictionaryValue* user_info;
    if (!args->GetDictionary(0u, &user_info) || !user_info) {
      ADD_FAILURE() << "Unabled to get event argument as dictionary for "
                    << "onUserInfoUpdated event.";
      return;
    }

    EXPECT_TRUE(user_info->GetString("userId", &user_id_));
    EXPECT_TRUE(user_info->GetBoolean("loggedIn", &user_logged_in_));
    EXPECT_TRUE(user_info->GetBoolean("dataReady", &user_data_ready_));
  }

  // Processes screenlockPrivate.onAuthAttempted event.
  void ConsumeAuthAttempted(base::ListValue* args) {
    ASSERT_TRUE(args);
    ASSERT_EQ(2u, args->GetSize());

    std::string auth_type;
    ASSERT_TRUE(args->GetString(0u, &auth_type));
    EXPECT_EQ("userClick", auth_type);
  }

  std::string user_id_;
  bool user_logged_in_;
  bool user_data_ready_;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockAppEventConsumer);
};

class EasyUnlockAppManagerTest : public testing::Test {
 public:
  EasyUnlockAppManagerTest() : command_line_(base::CommandLine::NO_PROGRAM) {}
  ~EasyUnlockAppManagerTest() override {}

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        proximity_auth::switches::kForceLoadEasyUnlockAppInTests);
    extensions::ExtensionSystem* extension_system = SetUpExtensionSystem();
    app_manager_ = EasyUnlockAppManager::Create(
        extension_system, IDR_EASY_UNLOCK_MANIFEST, GetAppPath());
  }

 protected:
  void SetExtensionSystemReady() {
    extensions::TestExtensionSystem* test_extension_system =
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(&profile_));
    test_extension_system->SetReady();
    base::RunLoop().RunUntilIdle();
  }

  base::FilePath GetAppPath() {
    return extensions::ExtensionPrefs::Get(&profile_)
        ->install_directory()
        .AppendASCII("easy_unlock");
  }

  int AuthAttemptedCount() const {
    return event_router_->GetEventCount(
        screenlock_private_api::OnAuthAttempted::kEventName);
  }

  int AppLaunchedCount() const {
    return event_router_->GetEventCount(
        app_runtime_api::OnLaunched::kEventName);
  }

 private:
  // Initializes test extension system.
  extensions::ExtensionSystem* SetUpExtensionSystem() {
    extensions::TestExtensionSystem* test_extension_system =
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(&profile_));
    extension_service_ = test_extension_system->CreateExtensionService(
        &command_line_, base::FilePath() /* install_directory */,
        false /* autoupdate_enabled */);

    extensions::ProcessManagerFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating(&CreateTestProcessManager));
    extensions::ScreenlockPrivateEventRouter::GetFactoryInstance()
        ->SetTestingFactory(
            &profile_,
            base::BindRepeating(&CreateScreenlockPrivateEventRouter));

    event_router_ = extensions::CreateAndUseTestEventRouter(&profile_);
    event_router_->AddEventObserver(&event_consumer_);
    event_router_->set_expected_extension_id(extension_misc::kEasyUnlockAppId);

    extension_service_->component_loader()->set_ignore_whitelist_for_testing(
        true);

    return test_extension_system;
  }

 protected:
  std::unique_ptr<EasyUnlockAppManager> app_manager_;

  // Needed by extension system.
  content::TestBrowserThreadBundle thread_bundle_;

  // Cros settings are needed when creating user manager.
  ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  // Needed for creating ExtensionService.
  ScopedTestUserManager test_user_manager_;

  TestingProfile profile_;

  EasyUnlockAppEventConsumer event_consumer_;
  extensions::ExtensionService* extension_service_;
  extensions::TestEventRouter* event_router_;

  base::CommandLine command_line_;

 private:
  DISALLOW_COPY_AND_ASSIGN(EasyUnlockAppManagerTest);
};

TEST_F(EasyUnlockAppManagerTest, LoadAppWhenNotLoaded) {
  SetExtensionSystemReady();

  // Sanity check for the test: the easy unlock app should not be loaded at
  // this point.
  ASSERT_FALSE(extension_service_->GetExtensionById(
      extension_misc::kEasyUnlockAppId, true));

  app_manager_->LoadApp();

  ASSERT_TRUE(extension_service_->GetExtensionById(
      extension_misc::kEasyUnlockAppId, false));
  EXPECT_TRUE(
      extension_service_->IsExtensionEnabled(extension_misc::kEasyUnlockAppId));
}

TEST_F(EasyUnlockAppManagerTest, LoadAppWhenAlreadyLoaded) {
  SetExtensionSystemReady();

  extension_service_->component_loader()->Add(IDR_EASY_UNLOCK_MANIFEST,
                                              GetAppPath());

  app_manager_->LoadApp();

  ASSERT_TRUE(extension_service_->GetExtensionById(
      extension_misc::kEasyUnlockAppId, false));
}

TEST_F(EasyUnlockAppManagerTest, LoadAppPreviouslyDisabled) {
  SetExtensionSystemReady();

  extension_service_->component_loader()->Add(IDR_EASY_UNLOCK_MANIFEST,
                                              GetAppPath());
  extension_service_->DisableExtension(
      extension_misc::kEasyUnlockAppId,
      extensions::disable_reason::DISABLE_RELOAD);

  ASSERT_TRUE(extension_service_->GetExtensionById(
      extension_misc::kEasyUnlockAppId, true));
  ASSERT_FALSE(
      extension_service_->IsExtensionEnabled(extension_misc::kEasyUnlockAppId));

  app_manager_->LoadApp();

  ASSERT_TRUE(extension_service_->GetExtensionById(
      extension_misc::kEasyUnlockAppId, false));
}

TEST_F(EasyUnlockAppManagerTest, ReloadApp) {
  SetExtensionSystemReady();

  extension_service_->component_loader()->Add(IDR_EASY_UNLOCK_MANIFEST,
                                              GetAppPath());

  ExtensionReloadTracker reload_tracker(&profile_,
                                        extension_misc::kEasyUnlockAppId);
  ASSERT_FALSE(reload_tracker.HasReloaded());

  app_manager_->ReloadApp();

  EXPECT_TRUE(reload_tracker.HasReloaded());
  EXPECT_TRUE(extension_service_->GetExtensionById(
      extension_misc::kEasyUnlockAppId, false));
}

TEST_F(EasyUnlockAppManagerTest, ReloadAppDisabled) {
  SetExtensionSystemReady();

  extension_service_->component_loader()->Add(IDR_EASY_UNLOCK_MANIFEST,
                                              GetAppPath());
  extension_service_->DisableExtension(
      extension_misc::kEasyUnlockAppId,
      extensions::disable_reason::DISABLE_RELOAD);
  ExtensionReloadTracker reload_tracker(&profile_,
                                        extension_misc::kEasyUnlockAppId);
  ASSERT_FALSE(reload_tracker.HasReloaded());

  app_manager_->ReloadApp();

  EXPECT_FALSE(reload_tracker.HasReloaded());
  EXPECT_TRUE(extension_service_->GetExtensionById(
      extension_misc::kEasyUnlockAppId, true));
  EXPECT_FALSE(
      extension_service_->IsExtensionEnabled(extension_misc::kEasyUnlockAppId));
}

TEST_F(EasyUnlockAppManagerTest, DisableApp) {
  SetExtensionSystemReady();

  extension_service_->component_loader()->Add(IDR_EASY_UNLOCK_MANIFEST,
                                              GetAppPath());
  EXPECT_TRUE(extension_service_->GetExtensionById(
      extension_misc::kEasyUnlockAppId, false));

  app_manager_->DisableAppIfLoaded();

  EXPECT_TRUE(extension_service_->GetExtensionById(
      extension_misc::kEasyUnlockAppId, true));
  EXPECT_FALSE(
      extension_service_->IsExtensionEnabled(extension_misc::kEasyUnlockAppId));
}

TEST_F(EasyUnlockAppManagerTest, DisableAppWhenNotLoaded) {
  SetExtensionSystemReady();

  EXPECT_FALSE(extension_service_->GetExtensionById(
      extension_misc::kEasyUnlockAppId, true));

  app_manager_->DisableAppIfLoaded();

  EXPECT_FALSE(extension_service_->GetExtensionById(
      extension_misc::kEasyUnlockAppId, true));

  extension_service_->component_loader()->Add(IDR_EASY_UNLOCK_MANIFEST,
                                              GetAppPath());
  EXPECT_TRUE(extension_service_->GetExtensionById(
      extension_misc::kEasyUnlockAppId, false));
}

TEST_F(EasyUnlockAppManagerTest, EnsureReady) {
  bool ready = false;
  app_manager_->EnsureReady(base::Bind(&VerifyFalseAndSetToTrue, &ready));

  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(ready);

  SetExtensionSystemReady();
  ASSERT_TRUE(ready);
}

TEST_F(EasyUnlockAppManagerTest, EnsureReadyAfterExtesionSystemReady) {
  SetExtensionSystemReady();

  bool ready = false;
  app_manager_->EnsureReady(base::Bind(&VerifyFalseAndSetToTrue, &ready));

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(ready);
}

TEST_F(EasyUnlockAppManagerTest, LaunchSetup) {
  SetExtensionSystemReady();

  ASSERT_EQ(0, AppLaunchedCount());

  app_manager_->LoadApp();
  app_manager_->LaunchSetup();

  EXPECT_EQ(1, AppLaunchedCount());
}

TEST_F(EasyUnlockAppManagerTest, LaunchSetupWhenDisabled) {
  SetExtensionSystemReady();

  ASSERT_EQ(0, AppLaunchedCount());

  app_manager_->LoadApp();
  app_manager_->DisableAppIfLoaded();

  app_manager_->LaunchSetup();

  EXPECT_EQ(0, AppLaunchedCount());
}

TEST_F(EasyUnlockAppManagerTest, LaunchSetupWhenNotLoaded) {
  SetExtensionSystemReady();

  ASSERT_EQ(0, AppLaunchedCount());

  app_manager_->LaunchSetup();

  EXPECT_EQ(0, AppLaunchedCount());
}

TEST_F(EasyUnlockAppManagerTest, SendAuthAttempted) {
  SetExtensionSystemReady();

  app_manager_->LoadApp();
  event_router_->AddLazyEventListener(
      screenlock_private_api::OnAuthAttempted::kEventName,
      extension_misc::kEasyUnlockAppId);

  EXPECT_TRUE(app_manager_->SendAuthAttemptEvent());
  EXPECT_EQ(1, AuthAttemptedCount());
}

TEST_F(EasyUnlockAppManagerTest, SendAuthAttemptedNoRegisteredListeners) {
  SetExtensionSystemReady();

  app_manager_->LoadApp();

  ASSERT_EQ(0, AuthAttemptedCount());

  EXPECT_FALSE(app_manager_->SendAuthAttemptEvent());
  EXPECT_EQ(0, AuthAttemptedCount());
}

TEST_F(EasyUnlockAppManagerTest, SendAuthAttemptedAppDisabled) {
  SetExtensionSystemReady();

  app_manager_->LoadApp();
  event_router_->AddLazyEventListener(
      screenlock_private_api::OnAuthAttempted::kEventName,
      extension_misc::kEasyUnlockAppId);
  app_manager_->DisableAppIfLoaded();

  ASSERT_EQ(0, AuthAttemptedCount());

  EXPECT_FALSE(app_manager_->SendAuthAttemptEvent());
  EXPECT_EQ(0, AuthAttemptedCount());
}

}  // namespace
}  // namespace chromeos
