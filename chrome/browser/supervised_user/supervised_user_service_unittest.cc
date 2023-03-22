// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_service.h"

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
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/manifest_constants.h"

using extensions::Extension;
#endif

using content::MessageLoopRunner;

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
    SupervisedUserService* service =
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
  SupervisedUserService* service =
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

#if !BUILDFLAG(ENABLE_EXTENSIONS)
TEST_F(SupervisedUserServiceTest, AreExtensionsPermissionsEnabled) {
  EXPECT_TRUE(profile_->IsChild());
  SupervisedUserService* service =
      SupervisedUserServiceFactory::GetForProfile(profile_.get());
  EXPECT_FALSE(service->AreExtensionsPermissionsEnabled());
}
#endif  // !BUILDFLAG(ENABLE_EXTENSIONS)

class SupervisedUserServiceTestUnsupervised
    : public SupervisedUserServiceTestBase {
 public:
  SupervisedUserServiceTestUnsupervised()
      : SupervisedUserServiceTestBase(/*is_supervised=*/false) {}
};

TEST_F(SupervisedUserServiceTestUnsupervised, IsURLFilteringEnabled) {
  SupervisedUserService* service =
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

#if !BUILDFLAG(ENABLE_EXTENSIONS)
TEST_F(SupervisedUserServiceTestUnsupervised, AreExtensionsPermissionsEnabled) {
  EXPECT_FALSE(profile_->IsChild());
  SupervisedUserService* service =
      SupervisedUserServiceFactory::GetForProfile(profile_.get());
  EXPECT_FALSE(service->AreExtensionsPermissionsEnabled());
}
#endif  // !BUILDFLAG(ENABLE_EXTENSIONS)

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

#if BUILDFLAG(ENABLE_EXTENSIONS)
class SupervisedUserServiceExtensionTestBase
    : public extensions::ExtensionServiceTestBase {
 public:
  explicit SupervisedUserServiceExtensionTestBase(bool is_supervised)
      : is_supervised_(is_supervised), channel_(version_info::Channel::DEV) {}
  ~SupervisedUserServiceExtensionTestBase() override {}

  void SetUp() override {
    ExtensionServiceTestBase::SetUp();
    ExtensionServiceTestBase::ExtensionServiceInitParams params =
        CreateDefaultInitParams();
    params.profile_is_supervised = is_supervised_;
    InitializeExtensionService(params);
    // Flush the message loop, to ensure that credentials have been loaded in
    // Identity Manager.
    base::RunLoop().RunUntilIdle();

    SupervisedUserService* service =
        SupervisedUserServiceFactory::GetForProfile(profile_.get());
    service->Init();

    supervised_user::SupervisedUserURLFilter* url_filter =
        service->GetURLFilter();
    url_filter->SetBlockingTaskRunnerForTesting(
        base::SingleThreadTaskRunner::GetCurrentDefault());
    url_filter_observer_.Init(url_filter);
  }

  void TearDown() override {
    // Flush the message loop, to ensure all posted tasks run.
    base::RunLoop().RunUntilIdle();
  }

 protected:
  scoped_refptr<const extensions::Extension> MakeThemeExtension() {
    base::Value::Dict source;
    source.Set(extensions::manifest_keys::kName, "Theme");
    source.Set(extensions::manifest_keys::kTheme, base::Value::Dict());
    source.Set(extensions::manifest_keys::kVersion, "1.0");
    extensions::ExtensionBuilder builder;
    scoped_refptr<const extensions::Extension> extension =
        builder.SetManifest(std::move(source)).Build();
    return extension;
  }

  scoped_refptr<const extensions::Extension> MakeExtension() {
    scoped_refptr<const extensions::Extension> extension =
        extensions::ExtensionBuilder("Extension").Build();
    return extension;
  }

  bool is_supervised_;
  extensions::ScopedCurrentChannel channel_;
  SupervisedUserURLFilterObserver url_filter_observer_;
};

class SupervisedUserServiceExtensionTestUnsupervised
    : public SupervisedUserServiceExtensionTestBase {
 public:
  SupervisedUserServiceExtensionTestUnsupervised()
      : SupervisedUserServiceExtensionTestBase(false) {}
};

TEST_F(SupervisedUserServiceExtensionTestUnsupervised,
       AreExtensionsPermissionsEnabled) {
  EXPECT_FALSE(profile_->IsChild());
  SupervisedUserService* service =
      SupervisedUserServiceFactory::GetForProfile(profile_.get());
  EXPECT_FALSE(service->AreExtensionsPermissionsEnabled());
}

class SupervisedUserServiceExtensionTest
    : public SupervisedUserServiceExtensionTestBase {
 public:
  SupervisedUserServiceExtensionTest()
      : SupervisedUserServiceExtensionTestBase(true) {}
};

TEST_F(SupervisedUserServiceExtensionTest,
       AreExtensionsPermissionsEnabledWithExtensionsPermissionsFlagDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      supervised_user::kEnableExtensionsPermissionsForSupervisedUsersOnDesktop);
  EXPECT_TRUE(profile_->IsChild());

  SupervisedUserService* service =
      SupervisedUserServiceFactory::GetForProfile(profile_.get());
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
  EXPECT_TRUE(service->AreExtensionsPermissionsEnabled());
#else
  EXPECT_FALSE(service->AreExtensionsPermissionsEnabled());
#endif
}

TEST_F(SupervisedUserServiceExtensionTest,
       AreExtensionsPermissionsEnabledWithExtensionsPermissionsFlagEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      supervised_user::kEnableExtensionsPermissionsForSupervisedUsersOnDesktop);
  EXPECT_TRUE(profile_->IsChild());

  SupervisedUserService* service =
      SupervisedUserServiceFactory::GetForProfile(profile_.get());
  EXPECT_TRUE(service->AreExtensionsPermissionsEnabled());
}

TEST_F(SupervisedUserServiceExtensionTest,
       ExtensionManagementPolicyProviderWithoutSUInitiatedInstalls) {
  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile_.get());
  supervised_user_test_util::
      SetSupervisedUserExtensionsMayRequestPermissionsPref(profile_.get(),
                                                           false);
  EXPECT_FALSE(profile_->GetPrefs()->GetBoolean(
      prefs::kSupervisedUserExtensionsMayRequestPermissions));
  EXPECT_TRUE(profile_->IsChild());

  // Check that a supervised user can install and uninstall a theme even if
  // they are not allowed to install extensions.
  {
    scoped_refptr<const extensions::Extension> theme = MakeThemeExtension();

    std::u16string error_1;
    EXPECT_TRUE(supervised_user_service->UserMayLoad(theme.get(), &error_1));
    EXPECT_TRUE(error_1.empty());

    std::u16string error_2;
    EXPECT_FALSE(
        supervised_user_service->MustRemainInstalled(theme.get(), &error_2));
    EXPECT_TRUE(error_2.empty());
  }

  // Now check a different kind of extension; the supervised user should not be
  // able to load it. It should also not need to remain installed.
  {
    scoped_refptr<const extensions::Extension> extension = MakeExtension();

    std::u16string error_1;
    EXPECT_FALSE(
        supervised_user_service->UserMayLoad(extension.get(), &error_1));
    EXPECT_FALSE(error_1.empty());

    std::u16string error_2;
    EXPECT_FALSE(
        supervised_user_service->UserMayInstall(extension.get(), &error_2));
    EXPECT_FALSE(error_2.empty());

    std::u16string error_3;
    EXPECT_FALSE(supervised_user_service->MustRemainInstalled(extension.get(),
                                                              &error_3));
    EXPECT_TRUE(error_3.empty());
  }

#if DCHECK_IS_ON()
  EXPECT_FALSE(supervised_user_service->GetDebugPolicyProviderName().empty());
#endif
}

TEST_F(SupervisedUserServiceExtensionTest,
       ExtensionManagementPolicyProviderWithSUInitiatedInstalls) {
  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile_.get());
  // Enable child users to initiate extension installs by simulating the
  // toggling of "Permissions for sites, apps and extensions" to enabled.
  supervised_user_test_util::
      SetSupervisedUserExtensionsMayRequestPermissionsPref(profile_.get(),
                                                           true);
  EXPECT_TRUE(profile_->GetPrefs()->GetBoolean(
      prefs::kSupervisedUserExtensionsMayRequestPermissions));
  EXPECT_TRUE(profile_->IsChild());

  // The supervised user should be able to load and uninstall the extensions
  // they install.
  {
    scoped_refptr<const extensions::Extension> extension = MakeExtension();

    std::u16string error;
    EXPECT_TRUE(supervised_user_service->UserMayLoad(extension.get(), &error));
    EXPECT_TRUE(error.empty());

    std::u16string error_2;
    EXPECT_FALSE(supervised_user_service->MustRemainInstalled(extension.get(),
                                                              &error_2));
    EXPECT_TRUE(error_2.empty());

    std::u16string error_3;
    extensions::disable_reason::DisableReason reason =
        extensions::disable_reason::DISABLE_NONE;
    EXPECT_TRUE(supervised_user_service->MustRemainDisabled(extension.get(),
                                                            &reason, &error_3));
    EXPECT_EQ(extensions::disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED,
              reason);
    EXPECT_FALSE(error_3.empty());

    std::u16string error_4;
    EXPECT_TRUE(supervised_user_service->UserMayModifySettings(extension.get(),
                                                               &error_4));
    EXPECT_TRUE(error_4.empty());

    std::u16string error_5;
    EXPECT_TRUE(
        supervised_user_service->UserMayInstall(extension.get(), &error_5));
    EXPECT_TRUE(error_5.empty());
  }

#if DCHECK_IS_ON()
  EXPECT_FALSE(supervised_user_service->GetDebugPolicyProviderName().empty());
#endif
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
