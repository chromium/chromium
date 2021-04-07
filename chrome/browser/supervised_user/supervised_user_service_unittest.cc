// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_service.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/supervised_user/permission_request_creator.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
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
  void Wait() {
    run_loop_->Run();
    Reset();
  }

 protected:
  AsyncTestHelper() {
    // |quit_called_| will be initialized in Reset().
    Reset();
  }

  ~AsyncTestHelper() {
    EXPECT_FALSE(quit_called_);
  }

  void QuitRunLoop() {
    // QuitRunLoop() can not be called more than once between calls to Wait().
    ASSERT_FALSE(quit_called_);
    quit_called_ = true;
    run_loop_->Quit();
  }

 private:
  void Reset() {
    quit_called_ = false;
    run_loop_.reset(new base::RunLoop);
  }

  std::unique_ptr<base::RunLoop> run_loop_;
  bool quit_called_;

  DISALLOW_COPY_AND_ASSIGN(AsyncTestHelper);
};

class SupervisedUserURLFilterObserver
    : public AsyncTestHelper,
      public SupervisedUserURLFilter::Observer {
 public:
  SupervisedUserURLFilterObserver() : scoped_observer_(this) {}
  ~SupervisedUserURLFilterObserver() {}

  void Init(SupervisedUserURLFilter* url_filter) {
    scoped_observer_.Add(url_filter);
  }

  // SupervisedUserURLFilter::Observer
  void OnSiteListUpdated() override {
    QuitRunLoop();
  }

 private:
  ScopedObserver<SupervisedUserURLFilter, SupervisedUserURLFilter::Observer>
      scoped_observer_;

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserURLFilterObserver);
};

class AsyncResultHolder {
 public:
  AsyncResultHolder() : result_(false) {}
  ~AsyncResultHolder() {}

  void SetResult(bool result) {
    result_ = result;
    run_loop_.Quit();
  }

  bool GetResult() {
    run_loop_.Run();
    return result_;
  }

 private:
  base::RunLoop run_loop_;
  bool result_;

  DISALLOW_COPY_AND_ASSIGN(AsyncResultHolder);
};

class SupervisedUserServiceTest : public ::testing::Test {
 public:
  SupervisedUserServiceTest() {}

  void SetUp() override {
    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment({});
    identity_test_environment_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());
    supervised_user_service_ =
        SupervisedUserServiceFactory::GetForProfile(profile_.get());
  }

  void TearDown() override {
    identity_test_environment_adaptor_.reset();
    profile_.reset();
  }

  ~SupervisedUserServiceTest() override {}

 protected:
  void AddURLAccessRequest(const GURL& url, AsyncResultHolder* result_holder) {
    supervised_user_service_->AddURLAccessRequest(
        url, base::BindOnce(&AsyncResultHolder::SetResult,
                            base::Unretained(result_holder)));
  }

  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_environment_adaptor_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  SupervisedUserService* supervised_user_service_;
};

}  // namespace

namespace {

class MockPermissionRequestCreator : public PermissionRequestCreator {
 public:
  MockPermissionRequestCreator() : enabled_(false) {}
  ~MockPermissionRequestCreator() override {}

  void set_enabled(bool enabled) {
    enabled_ = enabled;
  }

  const std::vector<GURL>& requested_urls() const {
    return requested_urls_;
  }

  void AnswerRequest(size_t index, bool result) {
    ASSERT_LT(index, requested_urls_.size());
    std::move(callbacks_[index]).Run(result);
    callbacks_.erase(callbacks_.begin() + index);
    requested_urls_.erase(requested_urls_.begin() + index);
  }

 private:
  // PermissionRequestCreator:
  bool IsEnabled() const override { return enabled_; }

  void CreateURLAccessRequest(const GURL& url_requested,
                              SuccessCallback callback) override {
    ASSERT_TRUE(enabled_);
    requested_urls_.push_back(url_requested);
    callbacks_.push_back(std::move(callback));
  }

  bool enabled_;
  std::vector<GURL> requested_urls_;
  std::vector<SuccessCallback> callbacks_;

  DISALLOW_COPY_AND_ASSIGN(MockPermissionRequestCreator);
};

}  // namespace

TEST_F(SupervisedUserServiceTest, CreatePermissionRequest) {
  GURL url("http://www.example.com");

  // Without any permission request creators, it should be disabled, and any
  // AddURLAccessRequest() calls should fail.
  EXPECT_FALSE(supervised_user_service_->AccessRequestsEnabled());
  {
    AsyncResultHolder result_holder;
    AddURLAccessRequest(url, &result_holder);
    EXPECT_FALSE(result_holder.GetResult());
  }

  // Add a disabled permission request creator. This should not change anything.
  MockPermissionRequestCreator* creator = new MockPermissionRequestCreator;
  supervised_user_service_->AddPermissionRequestCreator(
      base::WrapUnique(creator));

  EXPECT_FALSE(supervised_user_service_->AccessRequestsEnabled());
  {
    AsyncResultHolder result_holder;
    AddURLAccessRequest(url, &result_holder);
    EXPECT_FALSE(result_holder.GetResult());
  }

  // Enable the permission request creator. This should enable permission
  // requests and queue them up.
  creator->set_enabled(true);
  EXPECT_TRUE(supervised_user_service_->AccessRequestsEnabled());
  {
    AsyncResultHolder result_holder;
    AddURLAccessRequest(url, &result_holder);
    ASSERT_EQ(1u, creator->requested_urls().size());
    EXPECT_EQ(url.spec(), creator->requested_urls()[0].spec());

    creator->AnswerRequest(0, true);
    EXPECT_TRUE(result_holder.GetResult());
  }

  {
    AsyncResultHolder result_holder;
    AddURLAccessRequest(url, &result_holder);
    ASSERT_EQ(1u, creator->requested_urls().size());
    EXPECT_EQ(url.spec(), creator->requested_urls()[0].spec());

    creator->AnswerRequest(0, false);
    EXPECT_FALSE(result_holder.GetResult());
  }

  // Add a second permission request creator.
  MockPermissionRequestCreator* creator_2 = new MockPermissionRequestCreator;
  creator_2->set_enabled(true);
  supervised_user_service_->AddPermissionRequestCreator(
      base::WrapUnique(creator_2));

  {
    AsyncResultHolder result_holder;
    AddURLAccessRequest(url, &result_holder);
    ASSERT_EQ(1u, creator->requested_urls().size());
    EXPECT_EQ(url.spec(), creator->requested_urls()[0].spec());

    // Make the first creator succeed. This should make the whole thing succeed.
    creator->AnswerRequest(0, true);
    EXPECT_TRUE(result_holder.GetResult());
  }

  {
    AsyncResultHolder result_holder;
    AddURLAccessRequest(url, &result_holder);
    ASSERT_EQ(1u, creator->requested_urls().size());
    EXPECT_EQ(url.spec(), creator->requested_urls()[0].spec());

    // Make the first creator fail. This should fall back to the second one.
    creator->AnswerRequest(0, false);
    ASSERT_EQ(1u, creator_2->requested_urls().size());
    EXPECT_EQ(url.spec(), creator_2->requested_urls()[0].spec());

    // Make the second creator succeed, which will make the whole thing succeed.
    creator_2->AnswerRequest(0, true);
    EXPECT_TRUE(result_holder.GetResult());
  }
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
class SupervisedUserServiceExtensionTestBase
    : public extensions::ExtensionServiceTestBase {
 public:
  explicit SupervisedUserServiceExtensionTestBase(bool is_supervised)
      : is_supervised_(is_supervised),
        channel_(version_info::Channel::DEV) {}
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

    SupervisedUserURLFilter* url_filter = service->GetURLFilter();
    url_filter->SetBlockingTaskRunnerForTesting(
        base::ThreadTaskRunnerHandle::Get());
    url_filter_observer_.Init(url_filter);
  }

  void TearDown() override {
    // Flush the message loop, to ensure all posted tasks run.
    base::RunLoop().RunUntilIdle();
  }

 protected:
  scoped_refptr<const extensions::Extension> MakeThemeExtension() {
    std::unique_ptr<base::DictionaryValue> source(new base::DictionaryValue());
    source->SetString(extensions::manifest_keys::kName, "Theme");
    source->Set(extensions::manifest_keys::kTheme,
                std::make_unique<base::DictionaryValue>());
    source->SetString(extensions::manifest_keys::kVersion, "1.0");
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

class SupervisedUserServiceExtensionTest
    : public SupervisedUserServiceExtensionTestBase {
 public:
  SupervisedUserServiceExtensionTest()
      : SupervisedUserServiceExtensionTestBase(true) {}
};

TEST_F(SupervisedUserServiceExtensionTest,
       ExtensionManagementPolicyProviderWithoutSUInitiatedInstalls) {
  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile_.get());
  supervised_user_service
      ->SetSupervisedUserExtensionsMayRequestPermissionsPrefForTesting(false);
  EXPECT_FALSE(supervised_user_service
                   ->GetSupervisedUserExtensionsMayRequestPermissionsPref());
  EXPECT_TRUE(profile_->IsSupervised());

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
  supervised_user_service
      ->SetSupervisedUserExtensionsMayRequestPermissionsPrefForTesting(true);
  EXPECT_TRUE(supervised_user_service
                  ->GetSupervisedUserExtensionsMayRequestPermissionsPref());
  EXPECT_TRUE(profile_->IsSupervised());

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
                                                            &reason,
                                                            &error_3));
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
