// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/status_collector/app_info_generator.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/apps/app_service/publisher_host.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/session_manager/core/session_manager.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Matcher;
using ::testing::MatcherInterface;
using ::testing::Matches;
using ::testing::MatchResultListener;
using ::testing::Property;
using ::testing::ResultOf;

namespace em = enterprise_management;

namespace {

class TimePeriodMatcher : public MatcherInterface<const em::TimePeriod&> {
 public:
  explicit TimePeriodMatcher(const em::TimePeriod& time_period)
      : start_(base::Time::FromMillisecondsSinceUnixEpoch(
            time_period.start_timestamp())),
        end_(base::Time::FromMillisecondsSinceUnixEpoch(
            time_period.end_timestamp())) {}

  bool MatchAndExplain(const em::TimePeriod& time_period,
                       MatchResultListener* listener) const override {
    bool start_timestamp_equal =
        time_period.start_timestamp() == start_.InMillisecondsSinceUnixEpoch();
    if (!start_timestamp_equal) {
      *listener << " |start_timestamp| is "
                << base::Time::FromMillisecondsSinceUnixEpoch(
                       time_period.start_timestamp());
    }
    bool end_timestamp_equal =
        time_period.end_timestamp() == end_.InMillisecondsSinceUnixEpoch();
    if (!end_timestamp_equal) {
      *listener << " |end_timestamp| is "
                << base::Time::FromMillisecondsSinceUnixEpoch(
                       time_period.end_timestamp());
    }
    return start_timestamp_equal && end_timestamp_equal;
  }

  void DescribeTo(std::ostream* os) const override {
    *os << "equal to TimePeriod"
        << "(" << start_ << ", " << end_ << "]";
  }

 private:
  base::Time start_;
  base::Time end_;
};

auto EqApp(const std::string& app_id,
           const std::string& name,
           const em::AppInfo::Status status,
           const std::string& version,
           const em::AppInfo::AppType app_type,
           const std::vector<em::TimePeriod> app_activities = {}) {
  auto GetActivity =
      [](const em::AppInfo& input) -> std::vector<em::TimePeriod> {
    std::vector<em::TimePeriod> activities;
    for (const em::TimePeriod& time_period : input.active_time_periods()) {
      activities.push_back(time_period);
    }
    return activities;
  };

  std::vector<Matcher<const em::TimePeriod&>> activity_matchers;
  for (const em::TimePeriod& activity : app_activities) {
    activity_matchers.push_back(
        Matcher<const em::TimePeriod&>(new TimePeriodMatcher(activity)));
  }

  return AllOf(Property(&em::AppInfo::app_id, app_id),
               Property(&em::AppInfo::app_name, name),
               Property(&em::AppInfo::status, status),
               Property(&em::AppInfo::version, version),
               Property(&em::AppInfo::app_type, app_type),
               ResultOf(GetActivity, ElementsAreArray(activity_matchers)));
}

auto MakeActivity(const base::Time& start_time, const base::Time& end_time) {
  em::TimePeriod time_period;
  time_period.set_start_timestamp(start_time.InMillisecondsSinceUnixEpoch());
  time_period.set_end_timestamp(end_time.InMillisecondsSinceUnixEpoch());
  return time_period;
}

apps::AppPtr MakeApp(const std::string& app_id,
                     const std::string& name,
                     apps::Readiness readiness,
                     const std::string& version,
                     apps::AppType app_type) {
  auto app = std::make_unique<apps::App>(app_type, app_id);
  app->name = name;
  app->readiness = readiness;
  app->version = version;
  return app;
}

}  // namespace

namespace policy {

class AppInfoGeneratorTest : public ::testing::Test {
 public:
  AppInfoGeneratorTest() = default;

 protected:
  void PushApp(apps::AppPtr app) {
    apps::AppType app_type = app->app_type;
    std::vector<apps::AppPtr> deltas;
    deltas.push_back(std::move(app));
    AppServiceProxy()->OnApps(std::move(deltas), app_type,
                              /*should_notify_initialized=*/false);
  }

  void PushApp(const std::string& app_id,
               const std::string& name,
               apps::Readiness readiness,
               const std::string& version,
               apps::AppType app_type) {
    PushApp(MakeApp(app_id, name, readiness, version, app_type));
  }

  class Instance {
   public:
    explicit Instance(const std::string& app_id) {
      window_ = std::make_unique<aura::Window>(nullptr);
      window_->Init(ui::LAYER_NOT_DRAWN);
      instance_ = std::make_unique<apps::Instance>(
          app_id, base::UnguessableToken::Create(), window_.get());
    }

    apps::Instance* instance() const { return instance_.get(); }

    std::unique_ptr<apps::Instance> instance_;
    std::unique_ptr<aura::Window> window_;
  };

  void PushAppInstance(const Instance& instance, apps::InstanceState state) {
    auto time = test_clock_.Now();
    auto clone = instance.instance()->Clone();
    clone->UpdateState(state, time);

    GetInstanceRegistry().OnInstance(std::move(clone));
  }

  std::unique_ptr<TestingProfile> CreateProfile(const AccountId& account_id,
                                                bool is_affiliated = true) {
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName(account_id.GetUserEmail());
    auto profile = profile_builder.Build();
    user_manager_->AddUserWithAffiliationAndTypeAndProfile(
        account_id, is_affiliated, user_manager::UserType::kRegular,
        profile.get());
    return profile;
  }

  void SetUp() override {
    auto user_manager = std::make_unique<ash::FakeChromeUserManager>();
    user_manager_ = user_manager.get();
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));
    account_id_ = AccountId::FromUserEmail("affiliated@managed.com");
    profile_ = CreateProfile(account_id_);
    test_clock().SetNow(MakeLocalTime("25-MAR-2020 1:30am"));

    // Wait for AppServiceProxy to be ready.
    app_service_test_.SetUp(profile_.get());

    auto* provider = web_app::FakeWebAppProvider::Get(profile_.get());
    provider->SetStartSystemOnStart(true);
    provider->Start();

    app_registrar_ = &provider->GetRegistrarMutable();
  }

  apps::AppServiceProxy* AppServiceProxy() {
    return apps::AppServiceProxyFactory::GetForProfile(profile_.get());
  }

  apps::InstanceRegistry& GetInstanceRegistry() {
    return apps::AppServiceProxyFactory::GetForProfile(profile_.get())
        ->InstanceRegistry();
  }

  std::unique_ptr<AppInfoGenerator> GetGenerator(
      base::TimeDelta max_stored_past_activity_interval = base::Days(0)) {
    return std::make_unique<AppInfoGenerator>(
        nullptr, max_stored_past_activity_interval, &test_clock());
  }

  std::unique_ptr<AppInfoGenerator> GetReadyGenerator() {
    auto generator = GetGenerator();
    generator->OnLogin(profile());
    generator->OnReportingChanged(true);
    return generator;
  }

  void RegisterApp(std::unique_ptr<web_app::WebApp> web_app) {
    webapps::AppId app_id = web_app->app_id();
    DCHECK(!app_registrar_->GetAppById(app_id));
    app_registrar_->registry().emplace(std::move(app_id), std::move(web_app));
  }

  Profile* profile() { return profile_.get(); }

  ash::FakeChromeUserManager* user_manager() { return user_manager_; }

  AccountId account_id() { return account_id_; }

  static auto EqActivity(const base::Time& start_time,
                         const base::Time& end_time) {
    return AllOf(Property(&em::TimePeriod::start_timestamp,
                          start_time.InMillisecondsSinceUnixEpoch()),
                 Property(&em::TimePeriod::end_timestamp,
                          end_time.InMillisecondsSinceUnixEpoch()));
  }

  base::Time MakeLocalTime(const std::string& time_string) {
    base::Time time;
    EXPECT_TRUE(base::Time::FromString(time_string.c_str(), &time));
    return time;
  }

  base::Time MakeUTCTime(const std::string& time_string) {
    base::Time time;
    EXPECT_TRUE(base::Time::FromUTCString(time_string.c_str(), &time));
    return time;
  }

  base::SimpleTestClock& test_clock() { return test_clock_; }

 private:
  apps::ScopedOmitBorealisAppsForTesting scoped_omit_borealis_apps_for_testing_;
  apps::ScopedOmitBuiltInAppsForTesting scoped_omit_built_in_apps_for_testing_;
  apps::ScopedOmitPluginVmAppsForTesting
      scoped_omit_plugin_vm_apps_for_testing_;
  content::BrowserTaskEnvironment task_environment_;
  AccountId account_id_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<web_app::WebAppRegistrarMutable> app_registrar_;
  raw_ptr<ash::FakeChromeUserManager, DanglingUntriaged> user_manager_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
  TestingPrefServiceSimple pref_service_;

  session_manager::SessionManager session_manager_;

  base::SimpleTestClock test_clock_;
  apps::AppServiceTest app_service_test_;
};

TEST_F(AppInfoGeneratorTest, GenerateInventoryList) {
  PushApp("a", "FirstApp", apps::Readiness::kDisabledByPolicy, "1.1",
          apps::AppType::kArc);
  PushApp("b", "SecondApp", apps::Readiness::kReady, "1.2",
          apps::AppType::kChromeApp);
  PushApp("c", "ThirdApp", apps::Readiness::kUninstalledByUser, "",
          apps::AppType::kCrostini);

  user_manager()->LoginUser(account_id(), true);
  auto generator = GetReadyGenerator();
  auto result = generator->Generate();

  EXPECT_THAT(
      result.value(),
      ElementsAre(EqApp("a", "FirstApp", em::AppInfo_Status_STATUS_DISABLED,
                        "1.1", em::AppInfo_AppType_TYPE_ARC),
                  EqApp("b", "SecondApp", em::AppInfo_Status_STATUS_INSTALLED,
                        "1.2", em::AppInfo_AppType_TYPE_EXTENSION),
                  EqApp("c", "ThirdApp", em::AppInfo_Status_STATUS_UNINSTALLED,
                        "", em::AppInfo_AppType_TYPE_CROSTINI)));
}

TEST_F(AppInfoGeneratorTest, GenerateWebApp) {
  user_manager()->LoginUser(account_id(), true);
  auto generator = GetReadyGenerator();
  webapps::AppId app_id;
  {
    auto web_app = web_app::test::CreateWebApp(
        GURL("http://app.com/app/path"), web_app::WebAppManagement::kDefault);
    app_id = web_app->app_id();
    auto app =
        MakeApp(web_app->app_id(), "App", apps::Readiness::kUninstalledByUser,
                "", apps::AppType::kWeb);
    // For web apps, |publisher_id| is set to the start URL.
    app->publisher_id = web_app->start_url().spec();
    PushApp(std::move(app));
    RegisterApp(std::move(web_app));
  }

  Instance app_instance(app_id);
  test_clock().SetNow(MakeLocalTime("29-MAR-2020 3:30pm"));
  PushAppInstance(app_instance, apps::InstanceState::kStarted);
  test_clock().SetNow(MakeLocalTime("29-MAR-2020 8:30pm"));
  PushAppInstance(app_instance, apps::InstanceState::kDestroyed);

  test_clock().SetNow(MakeLocalTime("30-MAR-2020 11:00am"));
  auto result = generator->Generate();

  EXPECT_THAT(
      result.value(),
      ElementsAre(EqApp("http://app.com/", "http://app.com/",
                        em::AppInfo_Status_STATUS_UNINSTALLED, "",
                        em::AppInfo_AppType_TYPE_WEB,
                        {MakeActivity(MakeUTCTime("29-MAR-2020 12:00am"),
                                      MakeUTCTime("29-MAR-2020 5:00am"))})));
}

TEST_F(AppInfoGeneratorTest, GenerateSystemWebApp) {
  user_manager()->LoginUser(account_id(), true);
  auto generator = GetReadyGenerator();
  webapps::AppId app_id;
  {
    auto web_app = web_app::test::CreateWebApp(
        GURL("http://app.com/app/path"), web_app::WebAppManagement::kDefault);
    app_id = web_app->app_id();
    auto app =
        MakeApp(web_app->app_id(), "App", apps::Readiness::kUninstalledByUser,
                "", apps::AppType::kSystemWeb);
    // For system web apps, |publisher_id| is set to the start URL.
    app->publisher_id = web_app->start_url().spec();
    PushApp(std::move(app));
    RegisterApp(std::move(web_app));
  }

  Instance app_instance(app_id);
  test_clock().SetNow(MakeLocalTime("29-MAR-2020 3:30pm"));
  PushAppInstance(app_instance, apps::InstanceState::kStarted);
  test_clock().SetNow(MakeLocalTime("29-MAR-2020 8:30pm"));
  PushAppInstance(app_instance, apps::InstanceState::kDestroyed);

  test_clock().SetNow(MakeLocalTime("30-MAR-2020 11:00am"));
  auto result = generator->Generate();

  EXPECT_THAT(
      result.value(),
      ElementsAre(EqApp("http://app.com/", "http://app.com/",
                        em::AppInfo_Status_STATUS_UNINSTALLED, "",
                        em::AppInfo_AppType_TYPE_WEB,
                        {MakeActivity(MakeUTCTime("29-MAR-2020 12:00am"),
                                      MakeUTCTime("29-MAR-2020 5:00am"))})));
}

TEST_F(AppInfoGeneratorTest, MultipleInstances) {
  user_manager()->LoginUser(account_id(), true);
  auto generator = GetReadyGenerator();
  PushApp("a", "FirstApp", apps::Readiness::kDisabledByPolicy, "1.1",
          apps::AppType::kArc);
  Instance app_instance("a");
  Instance app_instance2("a");
  test_clock().SetNow(MakeLocalTime("29-MAR-2020 1:30pm"));
  PushAppInstance(app_instance, apps::InstanceState::kStarted);
  test_clock().SetNow(MakeLocalTime("29-MAR-2020 2:30pm"));
  PushAppInstance(app_instance2, apps::InstanceState::kStarted);
  test_clock().SetNow(MakeLocalTime("29-MAR-2020 3:30pm"));
  PushAppInstance(app_instance, apps::InstanceState::kDestroyed);
  test_clock().SetNow(MakeLocalTime("29-MAR-2020 4:30pm"));
  PushAppInstance(app_instance2, apps::InstanceState::kDestroyed);

  test_clock().SetNow(MakeLocalTime("30-MAR-2020 11:00am"));
  auto result = generator->Generate();

  EXPECT_THAT(
      result.value(),
      ElementsAre(EqApp("a", "FirstApp", em::AppInfo_Status_STATUS_DISABLED,
                        "1.1", em::AppInfo_AppType_TYPE_ARC,
                        {MakeActivity(MakeUTCTime("29-MAR-2020 12:00am"),
                                      MakeUTCTime("29-MAR-2020 3:00am"))})));
}

TEST_F(AppInfoGeneratorTest, ShouldNotReport) {
  user_manager()->LoginUser(account_id(), true);
  PushApp("a", "FirstApp", apps::Readiness::kDisabledByPolicy, "1.1",
          apps::AppType::kArc);

  auto generator = GetGenerator();
  generator->OnReportingChanged(false);
  generator->OnLogin(profile());
  auto result = generator->Generate();

  EXPECT_FALSE(result.has_value());
}

TEST_F(AppInfoGeneratorTest, UnaffiliatedUser) {
  auto unaffiliated_account_id =
      AccountId::FromUserEmail("unaffiliated@unmanaged.com");
  auto unaffiliated_profile =
      CreateProfile(unaffiliated_account_id, /* is_affiliated= */ false);
  user_manager()->LoginUser(unaffiliated_account_id, true);
  PushApp("a", "FirstApp", apps::Readiness::kDisabledByPolicy, "1.1",
          apps::AppType::kArc);

  auto generator = GetGenerator();
  generator->OnReportingChanged(true);
  generator->OnLogin(unaffiliated_profile.get());
  auto result = generator->Generate();

  EXPECT_FALSE(result.has_value());
}

TEST_F(AppInfoGeneratorTest, SecondaryUser) {
  user_manager()->LoginUser(account_id(), true);
  auto secondary_account_id = AccountId::FromUserEmail("secondary@managed.com");
  auto secondary_profile =
      CreateProfile(secondary_account_id, /* is_affiliated= */ true);
  user_manager()->LoginUser(secondary_account_id, true);
  PushApp("a", "FirstApp", apps::Readiness::kDisabledByPolicy, "1.1",
          apps::AppType::kArc);

  auto generator = GetGenerator();
  generator->OnReportingChanged(true);
  generator->OnLogin(secondary_profile.get());
  auto result = generator->Generate();

  EXPECT_FALSE(result.has_value());
}

TEST_F(AppInfoGeneratorTest, OnReportedSuccessfully) {
  user_manager()->LoginUser(account_id(), true);
  auto generator = GetReadyGenerator();
  PushApp("a", "FirstApp", apps::Readiness::kDisabledByPolicy, "1.1",
          apps::AppType::kArc);
  Instance app_instance("a");
  test_clock().SetNow(MakeLocalTime("29-MAR-2020 3:30pm"));
  PushAppInstance(app_instance, apps::InstanceState::kStarted);
  test_clock().SetNow(MakeLocalTime("30-MAR-2020 11:30pm"));
  PushAppInstance(app_instance, apps::InstanceState::kDestroyed);

  test_clock().SetNow(MakeLocalTime("30-MAR-2020 11:55pm"));
  auto report_time = test_clock().Now();
  test_clock().SetNow(MakeLocalTime("31-MAR-2020 12:10pm"));
  generator->OnReportedSuccessfully(report_time);

  test_clock().SetNow(MakeLocalTime("31-MAR-2020 5:30pm"));
  PushAppInstance(app_instance, apps::InstanceState::kStarted);
  test_clock().SetNow(MakeLocalTime("31-MAR-2020 11:30pm"));
  PushAppInstance(app_instance, apps::InstanceState::kDestroyed);
  auto result = generator->Generate();

  EXPECT_THAT(
      result.value(),
      ElementsAre(EqApp("a", "FirstApp", em::AppInfo_Status_STATUS_DISABLED,
                        "1.1", em::AppInfo_AppType_TYPE_ARC,
                        {MakeActivity(MakeUTCTime("31-MAR-2020 12:00am"),
                                      MakeUTCTime("31-MAR-2020 6:00am"))})));
}

TEST_F(AppInfoGeneratorTest, OnWillReport) {
  user_manager()->LoginUser(account_id(), true);
  auto generator = GetReadyGenerator();
  PushApp("a", "FirstApp", apps::Readiness::kDisabledByPolicy, "1.1",
          apps::AppType::kArc);
  Instance app_instance("a");
  test_clock().SetNow(MakeLocalTime("29-MAR-2020 3:30pm"));
  PushAppInstance(app_instance, static_cast<apps::InstanceState>(
                                    apps::InstanceState::kStarted |
                                    apps::InstanceState::kHidden));

  test_clock().SetNow(MakeLocalTime("30-MAR-2020 5:30pm"));
  generator->OnWillReport();
  auto result = generator->Generate();

  test_clock().SetNow(MakeLocalTime("31-MAR-2020 8:30pm"));
  PushAppInstance(app_instance, apps::InstanceState::kDestroyed);
  auto result2 = generator->Generate();

  EXPECT_THAT(
      result.value(),
      ElementsAre(EqApp("a", "FirstApp", em::AppInfo_Status_STATUS_DISABLED,
                        "1.1", em::AppInfo_AppType_TYPE_ARC,
                        {MakeActivity(MakeUTCTime("29-MAR-2020 12:00am"),
                                      MakeUTCTime("29-MAR-2020 8:30am")),
                         MakeActivity(MakeUTCTime("30-MAR-2020 12:00am"),
                                      MakeUTCTime("30-MAR-2020 5:30pm"))})));
  EXPECT_THAT(
      result2.value(),
      ElementsAre(EqApp("a", "FirstApp", em::AppInfo_Status_STATUS_DISABLED,
                        "1.1", em::AppInfo_AppType_TYPE_ARC,
                        {MakeActivity(MakeUTCTime("29-MAR-2020 12:00am"),
                                      MakeUTCTime("29-MAR-2020 8:30am")),
                         MakeActivity(MakeUTCTime("30-MAR-2020 12:00am"),
                                      MakeUTCTime("31-MAR-2020 12:00am")),
                         MakeActivity(MakeUTCTime("31-MAR-2020 12:00am"),
                                      MakeUTCTime("31-MAR-2020 8:30pm"))})));
}

TEST_F(AppInfoGeneratorTest, OnWillReport_DeviceLocked) {
  user_manager()->LoginUser(account_id(), true);
  auto generator = GetReadyGenerator();
  PushApp("a", "FirstApp", apps::Readiness::kDisabledByPolicy, "1.1",
          apps::AppType::kArc);
  Instance app_instance("a");
  test_clock().SetNow(MakeLocalTime("29-MAR-2020 3:00pm"));
  PushAppInstance(app_instance, apps::InstanceState::kStarted);

  test_clock().SetNow(MakeLocalTime("29-MAR-2020 4:00pm"));
  generator->OnLocked();

  test_clock().SetNow(MakeLocalTime("29-MAR-2020 5:00pm"));
  generator->OnWillReport();
  auto result1 = generator->Generate();

  test_clock().SetNow(MakeLocalTime("29-MAR-2020 7:00pm"));
  generator->OnUnlocked();

  test_clock().SetNow(MakeLocalTime("29-MAR-2020 7:30pm"));
  PushAppInstance(app_instance, apps::InstanceState::kDestroyed);
  auto result2 = generator->Generate();

  EXPECT_THAT(
      result1.value(),
      ElementsAre(EqApp("a", "FirstApp", em::AppInfo_Status_STATUS_DISABLED,
                        "1.1", em::AppInfo_AppType_TYPE_ARC,
                        {MakeActivity(MakeUTCTime("29-MAR-2020 12:00am"),
                                      MakeUTCTime("29-MAR-2020 1:00am"))})));

  EXPECT_THAT(
      result2.value(),
      ElementsAre(EqApp("a", "FirstApp", em::AppInfo_Status_STATUS_DISABLED,
                        "1.1", em::AppInfo_AppType_TYPE_ARC,
                        {MakeActivity(MakeUTCTime("29-MAR-2020 12:00am"),
                                      MakeUTCTime("29-MAR-2020 1:30am"))})));
}

TEST_F(AppInfoGeneratorTest, OnResumeActive_DeviceLocked) {
  user_manager()->LoginUser(account_id(), true);
  auto generator = GetReadyGenerator();
  PushApp("a", "FirstApp", apps::Readiness::kDisabledByPolicy, "1.1",
          apps::AppType::kArc);
  Instance app_instance("a");
  test_clock().SetNow(MakeLocalTime("29-MAR-2020 3:00pm"));
  PushAppInstance(app_instance, apps::InstanceState::kStarted);

  test_clock().SetNow(MakeLocalTime("29-MAR-2020 4:00pm"));
  generator->OnLocked();

  auto suspend_time = MakeLocalTime("29-MAR-2020 5:00pm");

  test_clock().SetNow(MakeLocalTime("29-MAR-2020 6:00pm"));
  generator->OnResumeActive(suspend_time);
  auto result1 = generator->Generate();

  test_clock().SetNow(MakeLocalTime("29-MAR-2020 7:00pm"));
  generator->OnUnlocked();

  test_clock().SetNow(MakeLocalTime("29-MAR-2020 7:30pm"));
  PushAppInstance(app_instance, apps::InstanceState::kDestroyed);
  auto result2 = generator->Generate();

  EXPECT_THAT(
      result1.value(),
      ElementsAre(EqApp("a", "FirstApp", em::AppInfo_Status_STATUS_DISABLED,
                        "1.1", em::AppInfo_AppType_TYPE_ARC,
                        {MakeActivity(MakeUTCTime("29-MAR-2020 12:00am"),
                                      MakeUTCTime("29-MAR-2020 1:00am"))})));

  EXPECT_THAT(
      result2.value(),
      ElementsAre(EqApp("a", "FirstApp", em::AppInfo_Status_STATUS_DISABLED,
                        "1.1", em::AppInfo_AppType_TYPE_ARC,
                        {MakeActivity(MakeUTCTime("29-MAR-2020 12:00am"),
                                      MakeUTCTime("29-MAR-2020 1:30am"))})));
}

TEST_F(AppInfoGeneratorTest, OnLogoutOnLogin) {
  user_manager()->LoginUser(account_id(), true);
  PushApp("a", "FirstApp", apps::Readiness::kDisabledByPolicy, "1.1",
          apps::AppType::kArc);
  auto generator = GetGenerator();
  generator->OnReportingChanged(true);
  generator->OnLogin(profile());
  generator->OnLogout(profile());
  Instance app_instance("a");
  test_clock().SetNow(MakeLocalTime("29-MAR-2020 1:30pm"));
  PushAppInstance(app_instance, apps::InstanceState::kStarted);
  test_clock().SetNow(MakeLocalTime("29-MAR-2020 3:30pm"));
  PushAppInstance(app_instance, apps::InstanceState::kDestroyed);

  test_clock().SetNow(MakeLocalTime("30-MAR-2020 11:00am"));
  auto result = generator->Generate();

  EXPECT_FALSE(result.has_value());

  generator->OnLogin(profile());

  test_clock().SetNow(MakeLocalTime("30-MAR-2020 2:30pm"));
  PushAppInstance(app_instance, apps::InstanceState::kStarted);
  test_clock().SetNow(MakeLocalTime("30-MAR-2020 5:30pm"));
  PushAppInstance(app_instance, apps::InstanceState::kDestroyed);

  test_clock().SetNow(MakeLocalTime("31-MAR-2020 11:00am"));
  auto result2 = generator->Generate();

  EXPECT_THAT(
      result2.value(),
      ElementsAre(EqApp("a", "FirstApp", em::AppInfo_Status_STATUS_DISABLED,
                        "1.1", em::AppInfo_AppType_TYPE_ARC,
                        {MakeActivity(MakeUTCTime("30-MAR-2020 12:00am"),
                                      MakeUTCTime("30-MAR-2020 3:00am"))})));
}

TEST_F(AppInfoGeneratorTest, OnLocked) {
  user_manager()->LoginUser(account_id(), true);
  auto generator = GetReadyGenerator();
  PushApp("a", "FirstApp", apps::Readiness::kDisabledByPolicy, "1.1",
          apps::AppType::kArc);
  Instance app_instance("a");
  test_clock().SetNow(MakeLocalTime("29-MAR-2020 3:30pm"));
  PushAppInstance(app_instance, apps::InstanceState::kStarted);

  test_clock().SetNow(MakeLocalTime("29-MAR-2020 5:30pm"));
  generator->OnLocked();

  test_clock().SetNow(MakeLocalTime("30-MAR-2020 11:00am"));
  auto result = generator->Generate();

  EXPECT_THAT(
      result.value(),
      ElementsAre(EqApp("a", "FirstApp", em::AppInfo_Status_STATUS_DISABLED,
                        "1.1", em::AppInfo_AppType_TYPE_ARC,
                        {MakeActivity(MakeUTCTime("29-MAR-2020 12:00am"),
                                      MakeUTCTime("29-MAR-2020 2:00am"))})));
}

TEST_F(AppInfoGeneratorTest, OnUnlocked) {
  user_manager()->LoginUser(account_id(), true);
  auto generator = GetReadyGenerator();
  PushApp("a", "FirstApp", apps::Readiness::kDisabledByPolicy, "1.1",
          apps::AppType::kArc);
  Instance app_instance("a");
  test_clock().SetNow(MakeLocalTime("29-MAR-2020 3:30pm"));
  PushAppInstance(app_instance, apps::InstanceState::kStarted);

  test_clock().SetNow(MakeLocalTime("29-MAR-2020 4:00pm"));
  generator->OnLocked();

  test_clock().SetNow(MakeLocalTime("29-MAR-2020 5:00pm"));
  generator->OnUnlocked();

  test_clock().SetNow(MakeLocalTime("29-MAR-2020 6:35pm"));
  PushAppInstance(app_instance, apps::InstanceState::kDestroyed);

  test_clock().SetNow(MakeLocalTime("30-MAR-2020 11:00am"));
  auto result = generator->Generate();

  EXPECT_THAT(
      result.value(),
      ElementsAre(EqApp("a", "FirstApp", em::AppInfo_Status_STATUS_DISABLED,
                        "1.1", em::AppInfo_AppType_TYPE_ARC,
                        {MakeActivity(MakeUTCTime("29-MAR-2020 12:00am"),
                                      MakeUTCTime("29-MAR-2020 2:05am"))})));
}

TEST_F(AppInfoGeneratorTest, OnResumeActive) {
  user_manager()->LoginUser(account_id(), true);
  auto generator = GetReadyGenerator();
  PushApp("a", "FirstApp", apps::Readiness::kDisabledByPolicy, "1.1",
          apps::AppType::kArc);
  Instance app_instance("a");
  test_clock().SetNow(MakeLocalTime("29-MAR-2020 3:30pm"));
  PushAppInstance(app_instance, apps::InstanceState::kStarted);

  auto suspend_time = MakeLocalTime("29-MAR-2020 4:00pm");
  test_clock().SetNow(suspend_time);

  test_clock().SetNow(MakeLocalTime("29-MAR-2020 5:00pm"));
  generator->OnResumeActive(suspend_time);

  test_clock().SetNow(MakeLocalTime("29-MAR-2020 6:35pm"));
  PushAppInstance(app_instance, apps::InstanceState::kDestroyed);

  test_clock().SetNow(MakeLocalTime("30-MAR-2020 11:00am"));
  auto result = generator->Generate();

  EXPECT_THAT(
      result.value(),
      ElementsAre(EqApp("a", "FirstApp", em::AppInfo_Status_STATUS_DISABLED,
                        "1.1", em::AppInfo_AppType_TYPE_ARC,
                        {MakeActivity(MakeUTCTime("29-MAR-2020 12:00am"),
                                      MakeUTCTime("29-MAR-2020 2:05am"))})));
}

TEST_F(AppInfoGeneratorTest, OnLoginRemoveOldUsage) {
  user_manager()->LoginUser(account_id(), true);
  PushApp("a", "FirstApp", apps::Readiness::kDisabledByPolicy, "1.1",
          apps::AppType::kArc);
  PushApp("b", "SecondApp", apps::Readiness::kReady, "1.2",
          apps::AppType::kChromeApp);
  auto max_days_past = base::Days(
      1);  // Exclude all past usage except for UTC today and yesterday.
  auto generator = GetGenerator(max_days_past);
  generator->OnReportingChanged(true);
  generator->OnLogin(profile());

  Instance app_instance1("a");
  test_clock().SetNow(MakeLocalTime("28-MAR-2020 1:30am"));
  PushAppInstance(app_instance1, apps::InstanceState::kStarted);
  test_clock().SetNow(MakeLocalTime("28-MAR-2020 3:30am"));
  PushAppInstance(app_instance1, apps::InstanceState::kDestroyed);
  Instance app_instance2("b");
  test_clock().SetNow(MakeLocalTime("29-MAR-2020 1:30am"));
  PushAppInstance(app_instance2, apps::InstanceState::kStarted);
  test_clock().SetNow(MakeLocalTime("29-MAR-2020 3:30am"));
  PushAppInstance(app_instance2, apps::InstanceState::kDestroyed);

  generator->OnLogout(profile());
  test_clock().SetNow(MakeLocalTime("30-MAR-2020 11:00am"));
  generator->OnLogin(profile());

  auto result = generator->Generate();

  EXPECT_THAT(
      result.value(),
      ElementsAre(EqApp("a", "FirstApp", em::AppInfo_Status_STATUS_DISABLED,
                        "1.1", em::AppInfo_AppType_TYPE_ARC),
                  EqApp("b", "SecondApp", em::AppInfo_Status_STATUS_INSTALLED,
                        "1.2", em::AppInfo_AppType_TYPE_EXTENSION,
                        {MakeActivity(MakeUTCTime("29-MAR-2020 12:00am"),
                                      MakeUTCTime("29-MAR-2020 2:00am"))})));
}

}  // namespace policy
