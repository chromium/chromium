// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_service.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_test_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::MessageLoopRunner;

namespace {

constexpr char kExampleHost0[] = "http://www.example0.com";
constexpr char kExampleURL1[] = "http://www.example1.com/123";

}  // namespace

namespace {

// Base class for helper objects that wait for certain events to happen.
// This class will ensure that calls to QuitRunLoop() (triggered by a subclass)
// are balanced with Wait() calls.
class AsyncTestHelper {
 public:
  AsyncTestHelper(const AsyncTestHelper&) = delete;
  AsyncTestHelper& operator=(const AsyncTestHelper&) = delete;

  void Wait() {
    run_loop_->Run();
    Reset();
  }

 protected:
  AsyncTestHelper() {
    // |quit_called_| will be initialized in Reset().
    Reset();
  }

  ~AsyncTestHelper() { EXPECT_FALSE(quit_called_); }

  void QuitRunLoop() {
    // QuitRunLoop() can not be called more than once between calls to Wait().
    ASSERT_FALSE(quit_called_);
    quit_called_ = true;
    run_loop_->Quit();
  }

 private:
  void Reset() {
    quit_called_ = false;
    run_loop_ = std::make_unique<base::RunLoop>();
  }

  std::unique_ptr<base::RunLoop> run_loop_;
  bool quit_called_;
};

class SupervisedUserURLFilterObserver
    : public AsyncTestHelper,
      public supervised_user::SupervisedUserURLFilter::Observer {
 public:
  SupervisedUserURLFilterObserver() {}

  SupervisedUserURLFilterObserver(const SupervisedUserURLFilterObserver&) =
      delete;
  SupervisedUserURLFilterObserver& operator=(
      const SupervisedUserURLFilterObserver&) = delete;

  ~SupervisedUserURLFilterObserver() {}

  void Init(supervised_user::SupervisedUserURLFilter* url_filter) {
    scoped_observation_.Observe(url_filter);
  }

  // SupervisedUserURLFilter::Observer
  void OnSiteListUpdated() override { QuitRunLoop(); }

 private:
  base::ScopedObservation<supervised_user::SupervisedUserURLFilter,
                          supervised_user::SupervisedUserURLFilter::Observer>
      scoped_observation_{this};
};

}  // namespace

class SupervisedUserServiceTestBase : public ::testing::Test {
 public:
  explicit SupervisedUserServiceTestBase(bool is_supervised) {
    // The testing browser process may be deleted following a crash.
    // Re-instantiate it before its use in testing profile creation.
    if (!g_browser_process) {
      TestingBrowserProcess::CreateInstance();
    }

    // Build supervised profile.
    TestingProfile::Builder builder;
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              SyncServiceFactory::GetDefaultFactory());
    if (is_supervised) {
      builder.SetIsSupervisedProfile();
    }
    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment(builder);
    identity_test_env_profile_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());
    supervised_user::SupervisedUserService* service =
        SupervisedUserServiceFactory::GetForProfile(profile_.get());
    service->Init();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;
};

class SupervisedUserServiceTest : public SupervisedUserServiceTestBase {
 public:
  SupervisedUserServiceTest()
      : SupervisedUserServiceTestBase(/*is_supervised=*/true) {}
};

TEST_F(SupervisedUserServiceTest, IsURLFilteringEnabled) {
  supervised_user::SupervisedUserService* service =
      SupervisedUserServiceFactory::GetForProfile(profile_.get());
  EXPECT_TRUE(profile_->IsChild());
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
  EXPECT_TRUE(service->IsURLFilteringEnabled());
#else
  EXPECT_FALSE(service->IsURLFilteringEnabled());
#endif

  // Enable filtering flag across platforms.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      supervised_user::kFilterWebsitesForSupervisedUsersOnDesktopAndIOS);

  EXPECT_TRUE(service->IsURLFilteringEnabled());
}

TEST_F(SupervisedUserServiceTest,
       AreExtensionsPermissionsEnabledWithExtensionsPermissionsFlagDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      supervised_user::kEnableExtensionsPermissionsForSupervisedUsersOnDesktop);
  EXPECT_TRUE(profile_->IsChild());

  supervised_user::SupervisedUserService* service =
      SupervisedUserServiceFactory::GetForProfile(profile_.get());
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_TRUE(service->AreExtensionsPermissionsEnabled());
#else
  EXPECT_FALSE(service->AreExtensionsPermissionsEnabled());
#endif
}

TEST_F(SupervisedUserServiceTest,
       AreExtensionsPermissionsEnabledWithExtensionsPermissionsFlagEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      supervised_user::kEnableExtensionsPermissionsForSupervisedUsersOnDesktop);
  EXPECT_TRUE(profile_->IsChild());

  supervised_user::SupervisedUserService* service =
      SupervisedUserServiceFactory::GetForProfile(profile_.get());
#if BUILDFLAG(ENABLE_EXTENSIONS)
  EXPECT_TRUE(service->AreExtensionsPermissionsEnabled());
#else
  EXPECT_FALSE(service->AreExtensionsPermissionsEnabled());
#endif
}

TEST_F(SupervisedUserServiceTest, ManagedSiteListTypeMetricOnPrefsChange) {
  base::HistogramTester histogram_tester;
  PrefService* prefs = profile_->GetPrefs();

  // Overriding the value of prefs::kSupervisedUserSafeSites and
  // prefs::kDefaultSupervisedUserFilteringBehavior in default storage is
  // needed, otherwise no report could be triggered by policies change. Since
  // the default values are the same of override values, the WebFilterType
  // doesn't change and no report here.
  prefs->SetInteger(prefs::kDefaultSupervisedUserFilteringBehavior,
                    supervised_user::SupervisedUserURLFilter::ALLOW);
  prefs->SetBoolean(prefs::kSupervisedUserSafeSites, true);

  // Blocks `kExampleHost0`.
  {
    ScopedDictPrefUpdate hosts_update(prefs, prefs::kSupervisedUserManualHosts);
    base::Value::Dict& hosts = hosts_update.Get();
    hosts.Set(kExampleHost0, false);
  }

  histogram_tester.ExpectBucketCount(
      supervised_user::SupervisedUserURLFilter::
          GetManagedSiteListHistogramNameForTest(),
      /*sample=*/
      supervised_user::SupervisedUserURLFilter::ManagedSiteList::
          kBlockedListOnly,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      supervised_user::SupervisedUserURLFilter::
          GetApprovedSitesCountHistogramNameForTest(),
      /*sample=*/0, /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      supervised_user::SupervisedUserURLFilter::
          GetBlockedSitesCountHistogramNameForTest(),
      /*sample=*/1, /*expected_count=*/1);

  // Approves `kExampleHost0`.
  {
    ScopedDictPrefUpdate hosts_update(prefs, prefs::kSupervisedUserManualHosts);
    base::Value::Dict& hosts = hosts_update.Get();
    hosts.Set(kExampleHost0, true);
  }

  histogram_tester.ExpectBucketCount(
      supervised_user::SupervisedUserURLFilter::
          GetManagedSiteListHistogramNameForTest(),
      /*sample=*/
      supervised_user::SupervisedUserURLFilter::ManagedSiteList::
          kApprovedListOnly,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      supervised_user::SupervisedUserURLFilter::
          GetApprovedSitesCountHistogramNameForTest(),
      /*sample=*/1, /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      supervised_user::SupervisedUserURLFilter::
          GetBlockedSitesCountHistogramNameForTest(),
      /*sample=*/0, /*expected_count=*/1);

  // Blocks `kExampleURL1`.
  {
    ScopedDictPrefUpdate urls_update(prefs, prefs::kSupervisedUserManualURLs);
    base::Value::Dict& urls = urls_update.Get();
    urls.Set(kExampleURL1, false);
  }

  histogram_tester.ExpectBucketCount(
      supervised_user::SupervisedUserURLFilter::
          GetManagedSiteListHistogramNameForTest(),
      /*sample=*/
      supervised_user::SupervisedUserURLFilter::ManagedSiteList::kBoth,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      supervised_user::SupervisedUserURLFilter::
          GetApprovedSitesCountHistogramNameForTest(),
      /*sample=*/1, /*expected_count=*/2);
  histogram_tester.ExpectBucketCount(
      supervised_user::SupervisedUserURLFilter::
          GetBlockedSitesCountHistogramNameForTest(),
      /*sample=*/1, /*expected_count=*/2);

  histogram_tester.ExpectTotalCount(
      supervised_user::SupervisedUserURLFilter::
          GetManagedSiteListHistogramNameForTest(),
      /*expected_count=*/3);
  histogram_tester.ExpectTotalCount(
      supervised_user::SupervisedUserURLFilter::
          GetApprovedSitesCountHistogramNameForTest(),
      /*expected_count=*/3);
  histogram_tester.ExpectTotalCount(
      supervised_user::SupervisedUserURLFilter::
          GetBlockedSitesCountHistogramNameForTest(),
      /*expected_count=*/3);
}

class SupervisedUserServiceTestUnsupervised
    : public SupervisedUserServiceTestBase {
 public:
  SupervisedUserServiceTestUnsupervised()
      : SupervisedUserServiceTestBase(/*is_supervised=*/false) {}
};

TEST_F(SupervisedUserServiceTestUnsupervised, IsURLFilteringEnabled) {
  supervised_user::SupervisedUserService* service =
      SupervisedUserServiceFactory::GetForProfile(profile_.get());
  EXPECT_FALSE(service->IsURLFilteringEnabled());

  // Enable filtering flag across platforms.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      supervised_user::kFilterWebsitesForSupervisedUsersOnDesktopAndIOS);
  EXPECT_TRUE(base::FeatureList::IsEnabled(
      supervised_user::kFilterWebsitesForSupervisedUsersOnDesktopAndIOS));

  EXPECT_FALSE(service->IsURLFilteringEnabled());
}

TEST_F(SupervisedUserServiceTestUnsupervised, AreExtensionsPermissionsEnabled) {
  EXPECT_FALSE(profile_->IsChild());
  supervised_user::SupervisedUserService* service =
      SupervisedUserServiceFactory::GetForProfile(profile_.get());
  EXPECT_FALSE(service->AreExtensionsPermissionsEnabled());
}

// TODO(crbug.com/1364589): Failing consistently on linux-chromeos-dbg
// due to failed timezone conversion assertion.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_DeprecatedFilterPolicy DISABLED_DeprecatedFilterPolicy
#else
#define MAYBE_DeprecatedFilterPolicy DeprecatedFilterPolicy
#endif
TEST_F(SupervisedUserServiceTest, MAYBE_DeprecatedFilterPolicy) {
  PrefService* prefs = profile_->GetPrefs();
  EXPECT_EQ(prefs->GetInteger(prefs::kDefaultSupervisedUserFilteringBehavior),
            supervised_user::SupervisedUserURLFilter::ALLOW);

  ASSERT_DCHECK_DEATH(
      prefs->SetInteger(prefs::kDefaultSupervisedUserFilteringBehavior,
                        /* SupervisedUserURLFilter::WARN */ 1));
}
