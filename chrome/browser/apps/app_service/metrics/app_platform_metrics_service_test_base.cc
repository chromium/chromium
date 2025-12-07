// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_service_test_base.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_service.h"
#include "chrome/browser/apps/app_service/publishers/app_publisher.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/login/users/scoped_account_id_annotator.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/trusted_vault/trusted_vault_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/account_id/account_id_literal.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/instance.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/user_manager/fake_user_manager_delegate.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/test_helper.h"
#include "content/public/browser/browser_context.h"
#include "ui/aura/window.h"

namespace apps {
namespace {

constexpr char kStartTime[] = "1 Jan 2021 21:00";
constexpr auto kTestUser =
    AccountId::Literal::FromUserEmailGaiaId("user@test.com",
                                            GaiaId::Literal("1234567890"));

std::unique_ptr<KeyedService> TestingSyncFactoryFunction(
    content::BrowserContext* context) {
  return std::make_unique<syncer::TestSyncService>();
}

user_manager::ScopedUserManager CreateUserManager() {
  return user_manager::ScopedUserManager(
      std::make_unique<user_manager::UserManagerImpl>(
          std::make_unique<user_manager::FakeUserManagerDelegate>(),
          TestingBrowserProcess::GetGlobal()->GetTestingLocalState()));
}

std::unique_ptr<TestingProfileManager> CreateTestingProfileManager() {
  return std::make_unique<TestingProfileManager>(
      TestingBrowserProcess::GetGlobal());
}

}  // namespace

TestApp::TestApp(std::string app_id,
                 AppType app_type,
                 std::string publisher_id,
                 Readiness readiness,
                 InstallReason install_reason,
                 InstallSource install_source,
                 bool should_notify_initialized,
                 bool is_platform_app,
                 WindowMode window_mode)
    : app_id(std::move(app_id)),
      app_type(app_type),
      publisher_id(std::move(publisher_id)),
      readiness(readiness),
      install_reason(install_reason),
      install_source(install_source),
      should_notify_initialized(should_notify_initialized),
      is_platform_app(is_platform_app),
      window_mode(window_mode) {}

TestApp::TestApp() = default;

TestApp::TestApp(const TestApp& other) = default;

TestApp::TestApp(TestApp&& other) = default;

AppPtr MakeApp(TestApp app) {
  auto result = AppPublisher::MakeApp(app.app_type, app.app_id, app.readiness,
                                      app.publisher_id, app.install_reason,
                                      app.install_source);
  result->publisher_id = app.publisher_id;
  result->is_platform_app = app.is_platform_app;
  result->window_mode = app.window_mode;
  return result;
}

void AddApp(AppServiceProxy* proxy, TestApp app) {
  CHECK(proxy);
  std::vector<AppPtr> deltas;
  deltas.push_back(MakeApp(app));
  proxy->OnApps(std::move(deltas), app.app_type, app.should_notify_initialized);
}

AppPlatformMetricsServiceTestBase::AppPlatformMetricsServiceTestBase()
    : user_manager_(CreateUserManager()),
      profile_manager_(CreateTestingProfileManager()) {}

AppPlatformMetricsServiceTestBase::~AppPlatformMetricsServiceTestBase() =
    default;

void AppPlatformMetricsServiceTestBase::SetUp() {
  ASSERT_TRUE(profile_manager_->SetUp());

  AddRegularUser(kTestUser);
  test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();

  base::Time start_time;
  EXPECT_TRUE(base::Time::FromUTCString(kStartTime, &start_time));
  base::TimeDelta forward_by = start_time - base::Time::Now();
  EXPECT_LT(base::TimeDelta(), forward_by);
  task_environment_.AdvanceClock(forward_by);
  GetPrefService()->SetInteger(
      kAppPlatformMetricsDayId,
      start_time.UTCMidnight().since_origin().InDaysFloored());

  if (!::chromeos::PowerManagerClient::Get()) {
    ::chromeos::PowerManagerClient::InitializeFake();
  }

  // Wait for AppServiceProxy to be ready.
  app_service_test_.SetUp(profile());

  app_platform_metrics_service_ =
      std::make_unique<AppPlatformMetricsService>(profile());

  if (start_app_platform_metrics_service_on_init_) {
    app_platform_metrics_service_->Start(
        AppServiceProxyFactory::GetForProfile(profile())->AppRegistryCache(),
        AppServiceProxyFactory::GetForProfile(profile())->InstanceRegistry(),
        AppServiceProxyFactory::GetForProfile(profile())
            ->AppCapabilityAccessCache());
  }
}

void AppPlatformMetricsServiceTestBase::TearDown() {
  user_manager_->Get()->OnUserProfileWillBeDestroyed(kTestUser);
  testing_profile_ = nullptr;
  sync_service_ = nullptr;
  app_platform_metrics_service_.reset();
  ::chromeos::PowerManagerClient::Shutdown();
}

void AppPlatformMetricsServiceTestBase::InstallOneApp(
    const std::string& app_id,
    AppType app_type,
    const std::string& publisher_id,
    Readiness readiness,
    InstallSource install_source,
    bool is_platform_app,
    WindowMode window_mode,
    InstallReason install_reason) {
  InstallOneApp({app_id, app_type, publisher_id, readiness, install_reason,
                 install_source,
                 /*should_notify_initialized=*/false, is_platform_app,
                 window_mode});
}

void AppPlatformMetricsServiceTestBase::InstallOneApp(TestApp app) {
  auto* proxy = AppServiceProxyFactory::GetForProfile(profile());
  AddApp(proxy, app);
}

void AppPlatformMetricsServiceTestBase::ResetAppPlatformMetricsService() {
  app_platform_metrics_service_.reset();
  app_platform_metrics_service_ =
      std::make_unique<AppPlatformMetricsService>(profile());

  app_platform_metrics_service_->Start(
      AppServiceProxyFactory::GetForProfile(profile())->AppRegistryCache(),
      AppServiceProxyFactory::GetForProfile(profile())->InstanceRegistry(),
      AppServiceProxyFactory::GetForProfile(profile())
          ->AppCapabilityAccessCache());
}

void AppPlatformMetricsServiceTestBase::ModifyInstance(
    const std::string& app_id,
    aura::Window* window,
    InstanceState state) {
  InstanceParams params(app_id, window);
  params.state = std::make_pair(state, base::Time::Now());
  AppServiceProxyFactory::GetForProfile(profile())
      ->InstanceRegistry()
      .CreateOrUpdateInstance(std::move(params));
}

void AppPlatformMetricsServiceTestBase::ModifyInstance(
    const base::UnguessableToken& instance_id,
    const std::string& app_id,
    aura::Window* window,
    InstanceState state) {
  auto instance = std::make_unique<Instance>(app_id, instance_id, window);
  instance->UpdateState(state, base::Time::Now());
  AppServiceProxyFactory::GetForProfile(profile())
      ->InstanceRegistry()
      .OnInstance(std::move(instance));
}

void AppPlatformMetricsServiceTestBase::ModifyWebAppInstance(
    const std::string& app_id,
    aura::Window* window,
    InstanceState state) {
  InstanceParams params(app_id, window);
  params.state = std::make_pair(state, base::Time::Now());
  AppServiceProxyFactory::GetForProfile(profile())
      ->InstanceRegistry()
      .CreateOrUpdateInstance(std::move(params));
}

sync_preferences::TestingPrefServiceSyncable*
AppPlatformMetricsServiceTestBase::GetPrefService() {
  return testing_profile_->GetTestingPrefService();
}

std::unique_ptr<AppPlatformMetricsService>
AppPlatformMetricsServiceTestBase::GetAppPlatformMetricsService() {
  return std::move(app_platform_metrics_service_);
}

int AppPlatformMetricsServiceTestBase::GetDayIdPref() {
  return GetPrefService()->GetInteger(kAppPlatformMetricsDayId);
}

TestingProfile::TestingFactories
AppPlatformMetricsServiceTestBase::GetTestingFactories() {
  return {
      TestingProfile::TestingFactory{
          TrustedVaultServiceFactory::GetInstance(),
          TrustedVaultServiceFactory::GetDefaultFactory()},
      TestingProfile::TestingFactory{SyncServiceFactory::GetInstance(),
                                     SyncServiceFactory::GetDefaultFactory()}};
}

void AppPlatformMetricsServiceTestBase::AddRegularUser(
    const AccountId::Literal& account_id) {
  CHECK(user_manager::TestHelper(user_manager_->Get())
            .AddRegularUser(account_id));
  user_manager_->Get()->UserLoggedIn(
      account_id, user_manager::TestHelper::GetFakeUsernameHash(account_id));
  ash::ScopedAccountIdAnnotator annotator(profile_manager_->profile_manager(),
                                          account_id);
  testing_profile_ = profile_manager_->CreateTestingProfile(
      std::string(account_id.GetUserEmail()), GetTestingFactories());

  user_manager_->Get()->OnUserProfileCreated(account_id,
                                             testing_profile_->GetPrefs());

  sync_service_ = static_cast<syncer::TestSyncService*>(
      SyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(), base::BindRepeating(&TestingSyncFactoryFunction)));
  sync_service_->SetInitialSyncFeatureSetupComplete(true);
}

}  // namespace apps
