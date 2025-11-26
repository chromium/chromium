// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/arc/arc_app_test.h"

#include <algorithm>
#include <vector>

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_service.h"
#include "chrome/browser/apps/app_service/publishers/arc_apps_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs_factory.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/privacy_items/arc_privacy_items_bridge.h"
#include "chrome/browser/ash/arc/session/arc_play_store_enabled_preference_handler.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/experiences/arc/arc_util.h"
#include "chromeos/ash/experiences/arc/intent_helper/arc_intent_helper_bridge.h"
#include "chromeos/ash/experiences/arc/mojom/app.mojom-shared.h"
#include "chromeos/ash/experiences/arc/session/arc_bridge_service.h"
#include "chromeos/ash/experiences/arc/session/arc_service_manager.h"
#include "chromeos/ash/experiences/arc/session/arc_session_runner.h"
#include "chromeos/ash/experiences/arc/test/arc_util_test_support.h"
#include "chromeos/ash/experiences/arc/test/connection_holder_util.h"
#include "chromeos/ash/experiences/arc/test/fake_app_instance.h"
#include "chromeos/ash/experiences/arc/test/fake_arc_session.h"
#include "chromeos/ash/experiences/arc/test/fake_compatibility_mode_instance.h"
#include "chromeos/ash/experiences/arc/test/fake_intent_helper_host.h"
#include "chromeos/ash/experiences/arc/test/fake_intent_helper_instance.h"
#include "components/session_manager/core/fake_session_manager_delegate.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/fake_user_manager_delegate.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/test_helper.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_impl.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kPackageName1[] = "fake.package.name1";
constexpr char kPackageName2[] = "fake.package.name2";
constexpr char kPackageName3[] = "fake.package.name3";
constexpr char kPackageName4[] = "fake.package.name4";
constexpr char kPackageName5[] = "fake.package.name5";

constexpr char kWebAppInfoTitle4[] = "package4";
constexpr char kWebAppInfoStartURL4[] = "https://example.com/app?start";
constexpr char kWebAppInfoScope4[] = "https://example.com/app";
constexpr char kWebAppInfoCertificateFingerprint4[] = "abc";

const std::vector<std::string> kSupportedLocales5 = {"en-US", "ja"};
constexpr char kSelectedLocale5[] = "en-US";

}  // namespace

// static
std::string ArcAppTest::GetAppId(const arc::mojom::AppInfo& app_info) {
  return ArcAppListPrefs::GetAppId(app_info.package_name, app_info.activity);
}

// static
std::string ArcAppTest::GetAppId(const arc::mojom::ShortcutInfo& shortcut) {
  return ArcAppListPrefs::GetAppId(shortcut.package_name, shortcut.intent_uri);
}

// static
std::vector<arc::mojom::ArcPackageInfoPtr> ArcAppTest::ClonePackages(
    const std::vector<arc::mojom::ArcPackageInfoPtr>& packages) {
  std::vector<arc::mojom::ArcPackageInfoPtr> result;
  for (const auto& package : packages)
    result.emplace_back(package->Clone());
  return result;
}

// static
std::vector<arc::mojom::AppInfoPtr> ArcAppTest::CloneApps(
    const std::vector<arc::mojom::AppInfoPtr>& apps) {
  std::vector<arc::mojom::AppInfoPtr> result;
  for (const auto& app : apps)
    result.emplace_back(app->Clone());
  return result;
}

ArcAppTest::ArcAppTest(UserManagerMode user_manager_mode)
    : user_manager_mode_(user_manager_mode) {
  CreateFakeAppsAndPackages();
}

ArcAppTest::~ArcAppTest() {
  CHECK(!need_pre_profile_teardown_);
  CHECK(!need_post_profile_teardown_);
}

void ArcAppTest::PreProfileSetUp() {
  CHECK(!is_pre_profile_setup_called_);
  is_pre_profile_setup_called_ = true;
  CHECK(!need_post_profile_teardown_);
  need_post_profile_teardown_ = true;

  arc::SetArcAvailableCommandLineForTesting(
      base::CommandLine::ForCurrentProcess());

  // TODO(crbug.com/455728516): Fix tests that create TestingProfile without
  // ProfileManager, and use ScopedAccountIdAnnotator.
  ash::ProfileHelper::SetProfileToUserForTestingEnabled(true);

  // ChromeMainDelegate::PostEarlyInitialization:
  if (!ash::ConciergeClient::Get()) {
    concierge_client_initialized_ = true;
    ash::ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);
  }

  // ChromeBrowserMainPartsAsh::PreCreateMainMessageLoop:
  if (user_manager_mode_ == UserManagerMode::kCreate) {
    CHECK(!session_manager::SessionManager::Get())
        << "Test should let ArcAppTest initializes SessionManager";
    session_manager_ = std::make_unique<session_manager::SessionManager>(
        std::make_unique<session_manager::FakeSessionManagerDelegate>());
  } else {
    CHECK(session_manager::SessionManager::Get())
        << "Test should initialize SessionManager too";
  }

  // ChromeBrowserMainPartsAsh::PreMainMessageLoopRun:
  arc_service_manager_ = std::make_unique<arc::ArcServiceManager>();
  // ConciergeClient must outlive ArcSessionManager.
  CHECK(ash::ConciergeClient::Get());
  arc_session_manager_ =
      arc::CreateTestArcSessionManager(std::make_unique<arc::ArcSessionRunner>(
          base::BindRepeating(arc::FakeArcSession::Create)));
  DCHECK(arc::ArcSessionManager::Get());
  arc::ArcSessionManager::SetUiEnabledForTesting(false);

  // ChromeBrowserMainPartsAsh::PreProfileInit:
  if (user_manager_mode_ == UserManagerMode::kCreate) {
    user_manager_.Reset(std::make_unique<user_manager::UserManagerImpl>(
        std::make_unique<user_manager::FakeUserManagerDelegate>(),
        TestingBrowserProcess::GetGlobal()->local_state()));
    session_manager::SessionManager::Get()->OnUserManagerCreated(
        user_manager_.Get());

    // Set up user and then create a session.
    CreateUserAndLogin();
  }
}

void ArcAppTest::PostProfileSetUp(Profile* profile) {
  CHECK(is_pre_profile_setup_called_);
  CHECK(!need_pre_profile_teardown_);
  need_pre_profile_teardown_ = true;

  DCHECK(!profile_);
  profile_ = profile;

  // Set up user-profile mapping.
  if (user_manager_mode_ == UserManagerMode::kCreate) {
    CHECK(user_);
    CHECK_EQ(user_->GetAccountId().GetUserEmail(),
             profile->GetProfileUserName())
        << "Test needs to call `SetUserEmail` before PreProfileSetUp";

    // NOTE: Some tests call SetUp() again after TearDown().
    // TODO(crbug.com/446582547): Fix those tests and remove the `Get` check.
    if (!ash::AnnotatedAccountId::Get(profile_)) {
      ash::AnnotatedAccountId::Set(profile_, user_->GetAccountId(),
                                   /*for_test=*/true);
    }

    user_manager_.Get()->OnUserProfileCreated(user_->GetAccountId(),
                                              profile->GetPrefs());

    // The testing mapping is needed for `ProfileHelper::GetProfileByUser`.
    // If the mapping is not setup, ProfileManager is required to look up the
    // profile. However, several test fixtures create TestingProfile without
    // ProfileManager.
    // TODO(crbug.com/455728516): Fix those tests and remove this.
    ash::ProfileHelper::Get()->SetUserToProfileMappingForTesting(user_,
                                                                 profile_);
  }

  arc::ResetArcAllowedCheckForTesting(profile_);

  // ArcServiceLauncher::MaybeSetProfile:
  CHECK(arc_session_manager_);
  arc_session_manager_->SetProfile(profile_);

  // ArcServiceLauncher::OnPrimaryUserProfilePrepared:
  {
    // Create keyed-services.

    // A valid |arc_app_list_prefs_| is needed for the ARC bridge service and
    // the ARC auth service.
    arc_app_list_pref_ = ArcAppListPrefs::Get(profile_);
    CHECK(arc_app_list_pref_);

    if (initialize_real_intent_helper_bridge_) {
      arc::ArcIntentHelperBridge::GetForBrowserContextForTesting(profile_);
      intent_helper_instance_ =
          std::make_unique<arc::FakeIntentHelperInstance>();
      arc_service_manager_->arc_bridge_service()->intent_helper()->SetInstance(
          intent_helper_instance_.get());
      WaitForInstanceReady(
          arc_service_manager_->arc_bridge_service()->intent_helper());
    }

    arc::ArcPrivacyItemsBridge::GetForBrowserContextForTesting(profile_);

    // Ensure that the singleton apps::ArcApps is constructed.
    apps::ArcAppsFactory::GetForProfile(profile_);

    arc_session_manager_->Initialize();

    arc_play_store_enabled_preference_handler_ =
        std::make_unique<arc::ArcPlayStoreEnabledPreferenceHandler>(
            profile_, arc_session_manager_.get());
    arc_play_store_enabled_preference_handler_->Start();
  }

  if (wait_default_apps_)
    WaitForDefaultApps();
  WaitForRemoveAllApps();

  apps::AppServiceProxyFactory::GetForProfile(profile_)
      ->PromiseAppService()
      ->SetSkipAlmanacForTesting(true);

  // Check initial conditions.
  if (activate_arc_on_start_) {
    if (!arc::ShouldArcAlwaysStart())
      arc::SetArcPlayStoreEnabledForProfile(profile_, true);
    if (!arc::IsArcPlayStoreEnabledPreferenceManagedForProfile(profile_))
      EXPECT_TRUE(arc_session_manager_->enable_requested());

    app_instance_ = std::make_unique<arc::FakeAppInstance>(arc_app_list_pref_);
    arc_service_manager_->arc_bridge_service()->app()->SetInstance(
        app_instance_.get());

    // TODO(khmel): Resolve this gracefully. Set of default app tests does not
    // expect waiting in ArcAppTest setup.
    if (wait_default_apps_)
      WaitForInstanceReady(arc_service_manager_->arc_bridge_service()->app());

    // Some test doesn't need to wait for compatibility_mode connection.
    if (wait_compatibility_mode_) {
      compatibility_mode_instance_ =
          std::make_unique<arc::FakeCompatibilityModeInstance>();
      arc_service_manager_->arc_bridge_service()
          ->compatibility_mode()
          ->SetInstance(compatibility_mode_instance_.get());
      WaitForInstanceReady(
          arc_service_manager_->arc_bridge_service()->compatibility_mode());
    }
  }
}

void ArcAppTest::WaitForDefaultApps() {
  DCHECK(arc_app_list_pref_);
  base::RunLoop run_loop;
  arc_app_list_pref_->SetDefaultAppsReadyCallback(run_loop.QuitClosure());
  run_loop.Run();
}

void ArcAppTest::WaitForRemoveAllApps() {
  DCHECK(arc_app_list_pref_);
  if (arc_app_list_pref_->is_remove_all_in_progress()) {
    base::RunLoop run_loop;
    arc_app_list_pref_->SetRemoveAllCallbackForTesting(run_loop.QuitClosure());
    run_loop.Run();
  }
}

void ArcAppTest::CreateFakeAppsAndPackages() {
  arc::mojom::AppInfo app;
  // Make sure we have enough data for test.
  for (int i = 1; i <= 5; ++i) {
    arc::mojom::AppInfoPtr app_info = arc::mojom::AppInfo::New(
        base::StringPrintf("Fake App %d", i),
        base::StringPrintf("fake.package.name%d", i),
        base::StringPrintf("fake.app.%d.activity", i), false /* sticky */);
    app_info->app_category = arc::mojom::AppCategory::kUndefined;
    fake_apps_.emplace_back(std::move(app_info));
  }
  fake_apps_[0]->sticky = true;

  for (int i = 1; i <= 3; ++i) {
    arc::mojom::AppInfoPtr app_info = arc::mojom::AppInfo::New(
        base::StringPrintf("TestApp%d", i), base::StringPrintf("test.app%d", i),
        base::StringPrintf("test.app%d.activity", i), true /* sticky */);
    app_info->app_category = arc::mojom::AppCategory::kUndefined;
    fake_default_apps_.emplace_back(std::move(app_info));
  }

  base::flat_map<arc::mojom::AppPermission, arc::mojom::PermissionStatePtr>
      permissions1;
  permissions1.emplace(arc::mojom::AppPermission::CAMERA,
                       arc::mojom::PermissionState::New(false /* granted */,
                                                        false /* managed */));
  fake_packages_.emplace_back(arc::mojom::ArcPackageInfo::New(
      kPackageName1 /* package_name */, 1 /* package_version */,
      1 /* last_backup_android_id */, 1 /* last_backup_time */,
      false /* sync */, false /* system */, false /* vpn_provider */,
      nullptr /* web_app_info */, std::nullopt, std::move(permissions1),
      std::nullopt /* version_name */, false /* preinstalled */));

  base::flat_map<arc::mojom::AppPermission, arc::mojom::PermissionStatePtr>
      permissions2;
  permissions2.emplace(arc::mojom::AppPermission::CAMERA,
                       arc::mojom::PermissionState::New(false /* granted */,
                                                        false /* managed */));
  permissions2.emplace(arc::mojom::AppPermission::MICROPHONE,
                       arc::mojom::PermissionState::New(false /* granted */,
                                                        false /* managed */));
  fake_packages_.emplace_back(arc::mojom::ArcPackageInfo::New(
      kPackageName2 /* package_name */, 2 /* package_version */,
      2 /* last_backup_android_id */, 2 /* last_backup_time */, true /* sync */,
      false /* system */, false /* vpn_provider */, nullptr /* web_app_info */,
      std::nullopt, std::move(permissions2), std::nullopt /* version_name */,
      false /* preinstalled */));

  base::flat_map<arc::mojom::AppPermission, arc::mojom::PermissionStatePtr>
      permissions3;
  permissions3.emplace(arc::mojom::AppPermission::CAMERA,
                       arc::mojom::PermissionState::New(false /* granted */,
                                                        false /* managed */));
  permissions3.emplace(arc::mojom::AppPermission::MICROPHONE,
                       arc::mojom::PermissionState::New(false /* granted */,
                                                        false /* managed */));
  permissions3.emplace(arc::mojom::AppPermission::LOCATION,
                       arc::mojom::PermissionState::New(true /* granted */,
                                                        false /* managed */));
  fake_packages_.emplace_back(arc::mojom::ArcPackageInfo::New(
      kPackageName3 /* package_name */, 3 /* package_version */,
      3 /* last_backup_android_id */, 3 /* last_backup_time */,
      false /* sync */, false /* system */, false /* vpn_provider */,
      nullptr /* web_app_info */, std::nullopt, std::move(permissions3),
      std::nullopt /* version_name */, false /* preinstalled */));

  base::flat_map<arc::mojom::AppPermission, arc::mojom::PermissionStatePtr>
      permissions4;
  fake_packages_.emplace_back(arc::mojom::ArcPackageInfo::New(
      kPackageName4,
      /*package_version=*/4,
      /*last_backup_android_id=*/4,
      /*last_backup_time=*/4,
      /*sync=*/false,
      /*system=*/false,
      /*vpn_provider=*/false,
      /*web_app_info=*/
      arc::mojom::WebAppInfo::New(kWebAppInfoTitle4, kWebAppInfoStartURL4,
                                  kWebAppInfoScope4,
                                  /*theme_color=*/0, /*is_web_only_twa=*/true,
                                  kWebAppInfoCertificateFingerprint4),
      /*deprecated_permissions=*/std::nullopt,
      /*permission_states=*/std::move(permissions4),
      /*version_name=*/std::nullopt,
      /*preinstalled=*/false));

  base::flat_map<arc::mojom::AppPermission, arc::mojom::PermissionStatePtr>
      permissions5;
  fake_packages_.emplace_back(arc::mojom::ArcPackageInfo::New(
      kPackageName5 /* package_name */, 5 /* package_version */,
      5 /* last_backup_android_id */, 5 /* last_backup_time */,
      false /* sync */, false /* system */, false /* vpn_provider */,
      nullptr /* web_app_info */, std::nullopt, std::move(permissions5),
      std::nullopt /* version_name */, false /* preinstalled */,
      arc::mojom::InstallPriority::kUndefined /* priority */,
      arc::mojom::PackageLocaleInfo::New(kSupportedLocales5,
                                         kSelectedLocale5)));

  for (int i = 1; i <= 5; ++i) {
    arc::mojom::ShortcutInfo shortcut_info;
    shortcut_info.name = base::StringPrintf("Fake Shortcut %d", i);
    shortcut_info.package_name = base::StringPrintf("fake.shortcut.%d", i);
    shortcut_info.intent_uri =
        base::StringPrintf("#Intent;fake.shortcut.%d.intent_uri", i);
    shortcut_info.icon_resource_id =
        base::StringPrintf("fake.shortcut.%d.icon_resource_id", i);
    fake_shortcuts_.push_back(shortcut_info);
  }
}

void ArcAppTest::PreProfileTearDown() {
  CHECK(need_pre_profile_teardown_);
  need_pre_profile_teardown_ = false;

  if (compatibility_mode_instance_) {
    compatibility_mode_instance_.reset();
  }
  app_instance_.reset();
  arc_play_store_enabled_preference_handler_.reset();

  CHECK(arc_session_manager_);
  arc_session_manager_->Shutdown();

  apps::ArcAppsFactory::GetInstance()->ShutDownForTesting(profile_);

  if (initialize_real_intent_helper_bridge_) {
    arc_service_manager_->arc_bridge_service()->intent_helper()->CloseInstance(
        intent_helper_instance_.get());
    intent_helper_instance_.reset();
    arc::ArcIntentHelperBridge::ShutDownForTesting(profile_);
  }

  arc::ArcPrivacyItemsBridge::ShutdownForTesting(profile_);

  arc::ResetArcAllowedCheckForTesting(profile_);

  if (user_manager_mode_ == UserManagerMode::kCreate) {
    CHECK(user_);
    user_manager_.Get()->OnUserProfileWillBeDestroyed(user_->GetAccountId());
  }

  profile_ = nullptr;
}

void ArcAppTest::PostProfileTearDown() {
  CHECK(!need_pre_profile_teardown_);
  CHECK(need_post_profile_teardown_);
  need_post_profile_teardown_ = false;
  is_pre_profile_setup_called_ = false;

  // ConciergeClient must outlive ArcSessionManager.
  CHECK(ash::ConciergeClient::Get());
  arc_session_manager_.reset();
  if (!persist_service_manager_) {
    arc_service_manager_.reset();
  }

  if (user_manager_mode_ == UserManagerMode::kCreate) {
    user_ = nullptr;

    CHECK(session_manager_);
    session_manager_.reset();

    CHECK(user_manager_.Get());
    user_manager_.Reset();
  }

  // ConciergeClient may be initialized from other testing utility, such as
  // ash::AshTestHelper::SetUp(), so Shutdown() only when it is initialized in
  // ArcAppTest::SetUp().
  if (concierge_client_initialized_) {
    ash::ConciergeClient::Shutdown();
    concierge_client_initialized_ = false;
  }

  // TODO(crbug.com/455728516): Fix tests that create TestingProfile without
  // ProfileManager, and use ScopedAccountIdAnnotator.
  ash::ProfileHelper::SetProfileToUserForTestingEnabled(false);
}

void ArcAppTest::StopArcInstance() {
  arc_service_manager_->arc_bridge_service()->app()->CloseInstance(
      app_instance_.get());
}

void ArcAppTest::RestartArcInstance() {
  auto* bridge_service = arc_service_manager_->arc_bridge_service();
  bridge_service->app()->CloseInstance(app_instance_.get());
  app_instance_ = std::make_unique<arc::FakeAppInstance>(arc_app_list_pref_);
  bridge_service->app()->SetInstance(app_instance_.get());
  WaitForInstanceReady(bridge_service->app());
}

void ArcAppTest::CreateUserAndLogin() {
  std::string profile_user_name = user_email_;
  if (user_email_.empty()) {
    profile_user_name = TestingProfile::kDefaultProfileUserName;
  }

  const AccountId account_id(
      AccountId::FromUserEmailGaiaId(profile_user_name, GaiaId("1234567890")));
  CHECK(user_manager::TestHelper(user_manager::UserManager::Get())
            .AddRegularUser(account_id));
  CHECK_DEREF(session_manager::SessionManager::Get())
      .CreateSession(account_id,
                     user_manager::TestHelper::GetFakeUsernameHash(account_id),
                     /*new_user=*/false, /*has_active_session=*/false);

  user_ = user_manager::UserManager::Get()->FindUser(account_id);
  CHECK(user_);
}

void ArcAppTest::AddPackage(arc::mojom::ArcPackageInfoPtr package) {
  if (FindPackage(package->package_name))
    return;
  fake_packages_.push_back(std::move(package));
}

void ArcAppTest::UpdatePackage(arc::mojom::ArcPackageInfoPtr updated_package) {
  auto it = std::ranges::find(fake_packages_, updated_package->package_name,
                              &arc::mojom::ArcPackageInfo::package_name);
  if (it != fake_packages_.end()) {
    *it = std::move(updated_package);
  }
}

void ArcAppTest::RemovePackage(const std::string& package_name) {
  std::erase_if(fake_packages_, [package_name](const auto& package) {
    return package->package_name == package_name;
  });
}

bool ArcAppTest::FindPackage(const std::string& package_name) {
  return base::Contains(fake_packages_, package_name,
                        &arc::mojom::ArcPackageInfo::package_name);
}

void ArcAppTest::SetUserEmail(const std::string& email) {
  user_email_ = email;
}
