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
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/instance.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/browser/browser_context.h"
#include "ui/aura/window.h"

namespace apps {
namespace {

constexpr char kStartTime[] = "1 Jan 2021 21:00";
constexpr char kTestUserEmail[] = "user@test.com";

std::unique_ptr<KeyedService> TestingSyncFactoryFunction(
    content::BrowserContext* context) {
  return std::make_unique<syncer::TestSyncService>();
}

}  // namespace

AppPtr MakeApp(const std::string& app_id,
               AppType app_type,
               const std::string& publisher_id,
               Readiness readiness,
               InstallReason install_reason,
               InstallSource install_source,
               bool is_platform_app,
               WindowMode window_mode) {
  auto app = AppPublisher::MakeApp(app_type, app_id, readiness, publisher_id,
                                   install_reason, install_source);
  app->publisher_id = publisher_id;
  app->is_platform_app = is_platform_app;
  app->window_mode = window_mode;
  return app;
}

void AddApp(AppRegistryCache& cache,
            const std::string& app_id,
            AppType app_type,
            const std::string& publisher_id,
            Readiness readiness,
            InstallReason install_reason,
            InstallSource install_source,
            bool should_notify_initialized,
            bool is_platform_app,
            WindowMode window_mode) {
  std::vector<AppPtr> deltas;
  deltas.push_back(MakeApp(app_id, app_type, publisher_id, readiness,
                           install_reason, install_source, is_platform_app,
                           window_mode));
  cache.OnApps(std::move(deltas), app_type, should_notify_initialized);
}

AppPlatformMetricsServiceTestBase::AppPlatformMetricsServiceTestBase() =
    default;

AppPlatformMetricsServiceTestBase::~AppPlatformMetricsServiceTestBase() =
    default;

void AppPlatformMetricsServiceTestBase::SetUp() {
  AddRegularUser(kTestUserEmail);
  test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();

  base::Time start_time;
  EXPECT_TRUE(base::Time::FromUTCString(kStartTime, &start_time));
  base::TimeDelta forward_by = start_time - base::Time::Now();
  EXPECT_LT(base::TimeDelta(), forward_by);
  task_environment_.AdvanceClock(forward_by);
  GetPrefService()->SetInteger(
      kAppPlatformMetricsDayId,
      start_time.UTCMidnight().since_origin().InDaysFloored());

  ::chromeos::PowerManagerClient::InitializeFake();

  app_platform_metrics_service_ =
      std::make_unique<AppPlatformMetricsService>(profile());

  // Install a BuiltIn app before app_platform_metrics_service_ started to
  // verify the install AppKM.
  AddApp(AppServiceProxyFactory::GetForProfile(profile())->AppRegistryCache(),
         /*app_id=*/"bu", AppType::kBuiltIn, "", Readiness::kReady,
         InstallReason::kSystem, InstallSource::kSystem,
         true /* should_notify_initialized */);

  app_platform_metrics_service_->Start(
      AppServiceProxyFactory::GetForProfile(profile())->AppRegistryCache(),
      AppServiceProxyFactory::GetForProfile(profile())->InstanceRegistry());
}

void AppPlatformMetricsServiceTestBase::TearDown() {
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
    WindowMode window_mode) {
  auto* proxy = AppServiceProxyFactory::GetForProfile(profile());
  AppRegistryCache& cache = proxy->AppRegistryCache();
  AddApp(cache, app_id, app_type, publisher_id, readiness, InstallReason::kUser,
         install_source, false /* should_notify_initialized */, is_platform_app,
         window_mode);
}

void AppPlatformMetricsServiceTestBase::ResetAppPlatformMetricsService() {
  app_platform_metrics_service_.reset();
  app_platform_metrics_service_ =
      std::make_unique<AppPlatformMetricsService>(profile());

  app_platform_metrics_service_->Start(
      AppServiceProxyFactory::GetForProfile(profile())->AppRegistryCache(),
      AppServiceProxyFactory::GetForProfile(profile())->InstanceRegistry());
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

void AppPlatformMetricsServiceTestBase::AddRegularUser(
    const std::string& email) {
  fake_user_manager_ = new ash::FakeChromeUserManager();
  scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
      base::WrapUnique(fake_user_manager_.get()));
  AccountId account_id = AccountId::FromUserEmail(email);
  const user_manager::User* user = fake_user_manager_->AddUser(account_id);
  fake_user_manager_->UserLoggedIn(account_id, user->username_hash(),
                                   /*browser_restart=*/false,
                                   /*is_child=*/false);
  fake_user_manager_->SimulateUserProfileLoad(account_id);

  TestingProfile::Builder builder;
  builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                            SyncServiceFactory::GetDefaultFactory());
  testing_profile_ = builder.Build();

  ash::ProfileHelper::Get()->SetUserToProfileMappingForTesting(user, profile());

  sync_service_ = static_cast<syncer::TestSyncService*>(
      SyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(), base::BindRepeating(&TestingSyncFactoryFunction)));
  sync_service_->SetFirstSetupComplete(true);
}

}  // namespace apps
