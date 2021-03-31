// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/status_collector/app_info_generator.h"

#include <memory>

#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/ash/login/users/chrome_user_manager.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/login/users/mock_user_manager.h"
#include "chrome/browser/web_applications/system_web_apps/test/test_system_web_app_manager.h"
#include "chrome/browser/web_applications/test/test_app_registrar.h"
#include "chrome/browser/web_applications/test/test_install_finalizer.h"
#include "chrome/browser/web_applications/test/test_web_app_provider.h"
#include "chrome/browser/web_applications/test/test_web_app_registry_controller.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/session_manager/core/session_manager.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
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
      : start_(base::Time::FromJavaTime(time_period.start_timestamp())),
        end_(base::Time::FromJavaTime(time_period.end_timestamp())) {}

  bool MatchAndExplain(const em::TimePeriod& time_period,
                       MatchResultListener* listener) const override {
    bool start_timestamp_equal =
        time_period.start_timestamp() == start_.ToJavaTime();
    if (!start_timestamp_equal) {
      *listener << " |start_timestamp| is "
                << base::Time::FromJavaTime(time_period.start_timestamp());
    }
    bool end_timestamp_equal = time_period.end_timestamp() == end_.ToJavaTime();
    if (!end_timestamp_equal) {
      *listener << " |end_timestamp| is "
                << base::Time::FromJavaTime(time_period.end_timestamp());
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
  time_period.set_start_timestamp(start_time.ToJavaTime());
  time_period.set_end_timestamp(end_time.ToJavaTime());
  return time_period;
}

}  // namespace

namespace policy {

class AppInfoGeneratorTest : public ::testing::Test {
 public:
  AppInfoGeneratorTest() = default;

 protected:
  void PushApp(const std::string& app_id,
               const std::string& name,
               const apps::mojom::Readiness readiness,
               const std::string& version,
               const apps::mojom::AppType app_type) {
    std::vector<apps::mojom::AppPtr> deltas;
    auto app = apps::mojom::App::New();
    app->app_id = app_id;
    app->name = name;
    app->readiness = readiness;
    app->version = version;
    app->app_type = app_type;
    deltas.push_back(std::move(app));
    GetCache().OnApps(std::move(deltas), app_type,
                      false /* should_notify_initialized */);
  }

  class Instance {
   public:
    explicit Instance(const std::string& app_id) {
      window_ = std::make_unique<aura::Window>(nullptr);
      window_->Init(ui::LAYER_NOT_DRAWN);
      instance_ = std::make_unique<apps::Instance>(app_id, window_.get());
    }

    apps::Instance* instance() const { return instance_.get(); }

    std::unique_ptr<apps::Instance> instance_;
    std::unique_ptr<aura::Window> window_;
  };

  void PushAppInstance(const Instance& instance, apps::InstanceState state) {
    auto time = test_clock_.Now();
    auto clone = instance.instance()->Clone();
    clone->UpdateState(state, time);

    std::vector<std::unique_ptr<apps::Instance>> deltas;
    deltas.push_back(std::move(clone));
    GetInstanceRegistry().OnInstances(deltas);
  }

  void SetUp() override {
    auto user_manager = std::make_unique<ash::FakeChromeUserManager>();
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));
    profile_ = std::make_unique<TestingProfile>();
    test_clock().SetNow(MakeLocalTime("25-MAR-2020 1:30am"));

    web_app::WebAppProviderFactory::GetInstance()->SetTestingFactoryAndUse(
        profile_.get(),
        base::BindLambdaForTesting([this](content::BrowserContext* context)
                                       -> std::unique_ptr<KeyedService> {
          Profile* profile = Profile::FromBrowserContext(context);
          auto provider =
              std::make_unique<web_app::TestWebAppProvider>(profile);
          auto app_registrar = std::make_unique<web_app::TestAppRegistrar>();
          auto system_web_app_manager =
              std::make_unique<web_app::TestSystemWebAppManager>(profile);

          app_registrar_ = app_registrar.get();
          provider->SetRegistrar(std::move(app_registrar));
          provider->SetSystemWebAppManager(std::move(system_web_app_manager));
          provider->Start();
          return provider;
        }));
  }

  apps::AppRegistryCache& GetCache() {
    return apps::AppServiceProxyFactory::GetForProfile(profile_.get())
        ->AppRegistryCache();
  }

  apps::InstanceRegistry& GetInstanceRegistry() {
    return apps::AppServiceProxyFactory::GetForProfile(profile_.get())
        ->InstanceRegistry();
  }

  std::unique_ptr<AppInfoGenerator> GetGenerator(
      base::TimeDelta max_stored_past_activity_interval =
          base::TimeDelta::FromDays(0)) {
    return std::make_unique<AppInfoGenerator>(max_stored_past_activity_interval,
                                              &test_clock());
  }

  std::unique_ptr<AppInfoGenerator> GetReadyGenerator() {
    auto generator = GetGenerator();
    generator->OnAffiliatedLogin(profile());
    generator->OnReportingChanged(true);
    return generator;
  }

  web_app::TestAppRegistrar* web_app_registrar() { return app_registrar_; }

  Profile* profile() { return profile_.get(); }

  static auto EqActivity(const base::Time& start_time,
                         const base::Time& end_time) {
    return AllOf(
        Property(&em::TimePeriod::start_timestamp, start_time.ToJavaTime()),
        Property(&em::TimePeriod::end_timestamp, end_time.ToJavaTime()));
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
  apps::ScopedOmitBuiltInAppsForTesting scoped_omit_built_in_apps_for_testing_;
  apps::ScopedOmitPluginVmAppsForTesting
      scoped_omit_plugin_vm_apps_for_testing_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  web_app::TestAppRegistrar* app_registrar_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
  TestingPrefServiceSimple pref_service_;

  session_manager::SessionManager session_manager_;

  base::SimpleTestClock test_clock_;
};

TEST_F(AppInfoGeneratorTest, GenerateInventoryList) {
  PushApp("a", "FirstApp", apps::mojom::Readiness::kDisabledByPolicy, "1.1",
          apps::mojom::AppType::kArc);
  PushApp("b", "SecondApp", apps::mojom::Readiness::kReady, "1.2",
          apps::mojom::AppType::kExtension);
  PushApp("c", "ThirdApp", apps::mojom::Readiness::kUninstalledByUser, "",
          apps::mojom::AppType::kCrostini);

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
  auto generator = GetReadyGenerator();
  PushApp("c", "App", apps::mojom::Readiness::kUninstalledByUser, "",
          apps::mojom::AppType::kWeb);
  web_app::TestAppRegistrar::AppInfo app = {
      GURL::EmptyGURL(), web_app::ExternalInstallSource::kExternalDefault,
      GURL("http://app.com/app")};
  web_app_registrar()->AddExternalApp("c", app);
  Instance app_instance("c");
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
  auto generator = GetReadyGenerator();
  PushApp("a", "FirstApp", apps::mojom::Readiness::kDisabledByPolicy, "1.1",
          apps::mojom::AppType::kArc);
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
  PushApp("a", "FirstApp", apps::mojom::Readiness::kDisabledByPolicy, "1.1",
          apps::mojom::AppType::kArc);

  auto generator = GetGenerator();
  generator->OnReportingChanged(false);
  generator->OnAffiliatedLogin(profile());
  auto result = generator->Generate();

  EXPECT_FALSE(result.has_value());
}

TEST_F(AppInfoGeneratorTest, OnReportedSuccessfully) {
  auto generator = GetReadyGenerator();
  PushApp("a", "FirstApp", apps::mojom::Readiness::kDisabledByPolicy, "1.1",
          apps::mojom::AppType::kArc);
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
  auto generator = GetReadyGenerator();
  PushApp("a", "FirstApp", apps::mojom::Readiness::kDisabledByPolicy, "1.1",
          apps::mojom::AppType::kArc);
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

TEST_F(AppInfoGeneratorTest, OnLogoutOnLogin) {
  PushApp("a", "FirstApp", apps::mojom::Readiness::kDisabledByPolicy, "1.1",
          apps::mojom::AppType::kArc);
  auto generator = GetGenerator();
  generator->OnReportingChanged(true);
  generator->OnAffiliatedLogin(profile());
  generator->OnAffiliatedLogout(profile());
  Instance app_instance("a");
  test_clock().SetNow(MakeLocalTime("29-MAR-2020 1:30pm"));
  PushAppInstance(app_instance, apps::InstanceState::kStarted);
  test_clock().SetNow(MakeLocalTime("29-MAR-2020 3:30pm"));
  PushAppInstance(app_instance, apps::InstanceState::kDestroyed);

  test_clock().SetNow(MakeLocalTime("30-MAR-2020 11:00am"));
  auto result = generator->Generate();

  EXPECT_FALSE(result.has_value());

  generator->OnAffiliatedLogin(profile());

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
  auto generator = GetReadyGenerator();
  PushApp("a", "FirstApp", apps::mojom::Readiness::kDisabledByPolicy, "1.1",
          apps::mojom::AppType::kArc);
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
  auto generator = GetReadyGenerator();
  PushApp("a", "FirstApp", apps::mojom::Readiness::kDisabledByPolicy, "1.1",
          apps::mojom::AppType::kArc);
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
  auto generator = GetReadyGenerator();
  PushApp("a", "FirstApp", apps::mojom::Readiness::kDisabledByPolicy, "1.1",
          apps::mojom::AppType::kArc);
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
  PushApp("a", "FirstApp", apps::mojom::Readiness::kDisabledByPolicy, "1.1",
          apps::mojom::AppType::kArc);
  PushApp("b", "SecondApp", apps::mojom::Readiness::kReady, "1.2",
          apps::mojom::AppType::kExtension);
  auto max_days_past = base::TimeDelta::FromDays(
      1);  // Exclude all past usage except for UTC today and yesterday.
  auto generator = GetGenerator(max_days_past);
  generator->OnReportingChanged(true);
  generator->OnAffiliatedLogin(profile());

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

  generator->OnAffiliatedLogout(profile());
  test_clock().SetNow(MakeLocalTime("30-MAR-2020 11:00am"));
  generator->OnAffiliatedLogin(profile());

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
