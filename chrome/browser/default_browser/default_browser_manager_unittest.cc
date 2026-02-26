// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/default_browser_manager.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/metrics/statistics_recorder.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/default_browser/default_browser_controller.h"
#include "chrome/browser/default_browser/default_browser_features.h"
#include "chrome/browser/default_browser/test_support/fake_shell_delegate.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/test/base/testing_browser_process.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/user_data_factory.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#endif  // BUILDFLAG(IS_WIN)

namespace default_browser {

namespace {

#if BUILDFLAG(IS_WIN)
constexpr wchar_t kRegistryPath[] =
    L"Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\h"
    L"ttp\\UserChoice";
constexpr wchar_t kProgIdValue[] = L"ProgId";

void CreateDefaultBrowserKey(const std::wstring& prog_id) {
  base::win::RegKey key(HKEY_CURRENT_USER, kRegistryPath,
                        KEY_WRITE | KEY_CREATE_SUB_KEY);
  ASSERT_TRUE(key.Valid());
  ASSERT_EQ(key.WriteValue(kProgIdValue, prog_id.c_str()), ERROR_SUCCESS);
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace

class DefaultBrowserManagerTest : public testing::Test {
 protected:
  DefaultBrowserManagerTest() {
    scoped_feature_list_.InitAndEnableFeature(
        kPerformDefaultBrowserCheckValidations);
  }

  ~DefaultBrowserManagerTest() override = default;

  void SetUp() override {
#if BUILDFLAG(IS_WIN)
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER));
    base::win::RegKey key;
    ASSERT_EQ(ERROR_SUCCESS,
              key.Create(HKEY_CURRENT_USER, kRegistryPath, KEY_WRITE));
#endif

    global_feature_override_ =
        GlobalFeatures::GetUserDataFactoryForTesting().AddOverrideForTesting(
            base::BindLambdaForTesting([&](BrowserProcess& browser_process) {
              auto fake_shell_delegate = std::make_unique<FakeShellDelegate>();
              fake_shell_delegate_ptr_ = fake_shell_delegate.get();
              return std::make_unique<DefaultBrowserManager>(
                  TestingBrowserProcess::GetGlobal(),
                  std::move(fake_shell_delegate),
                  base::BindLambdaForTesting(
                      [&]() { return static_cast<Profile*>(&profile_); }));
            }));

    TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
        /*profile_manager=*/false);
  }

  void TearDown() override {
    fake_shell_delegate_ptr_ = nullptr;
    TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();
  }

  DefaultBrowserManager& default_browser_manager() {
    return *DefaultBrowserManager::From(TestingBrowserProcess::GetGlobal());
  }

  FakeShellDelegate& shell_delegate() { return *fake_shell_delegate_ptr_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
#if BUILDFLAG(IS_WIN)
  registry_util::RegistryOverrideManager registry_override_manager_;
#endif

  ui::UserDataFactory::ScopedOverride global_feature_override_;
  raw_ptr<FakeShellDelegate> fake_shell_delegate_ptr_;
  TestingProfile profile_;
};

TEST_F(DefaultBrowserManagerTest, GetDefaultBrowserState) {
  base::test::TestFuture<DefaultBrowserState> future;
  shell_delegate().set_default_state(shell_integration::IS_DEFAULT);
  default_browser_manager().GetDefaultBrowserState(future.GetCallback());

  ASSERT_TRUE(future.Wait()) << "GetDefaultBrowserState should trigger the "
                                "callback after fetching default browser state";
  EXPECT_EQ(future.Get(), shell_integration::IS_DEFAULT);
}

TEST_F(DefaultBrowserManagerTest, CreateControllerForSettingsPage) {
  auto controller = DefaultBrowserManager::CreateControllerFor(
      DefaultBrowserEntrypointType::kSettingsPage);

  ASSERT_NE(controller, nullptr);
  EXPECT_EQ(controller->GetSetterType(),
            DefaultBrowserSetterType::kShellIntegration);
}

TEST_F(DefaultBrowserManagerTest, CreateControllerForStartupInfobar) {
  auto controller = DefaultBrowserManager::CreateControllerFor(
      DefaultBrowserEntrypointType::kStartupInfobar);

  ASSERT_NE(controller, nullptr);
  EXPECT_EQ(controller->GetSetterType(),
            DefaultBrowserSetterType::kShellIntegration);
}

#if BUILDFLAG(IS_WIN)
constexpr std::string_view kHttpProgIdValidationHistogramName =
    "DefaultBrowser.HttpProgIdAssocValidationResult";

TEST_F(DefaultBrowserManagerTest,
       DefaultBrowserCheckTruePositiveWithHTTPAssoc) {
  shell_delegate().set_default_state(shell_integration::IS_DEFAULT);
  shell_delegate().set_http_assoc_prog_id(u"ChromeHTML");

  base::HistogramTester histogram_tester;
  default_browser_manager().GetDefaultBrowserState(base::DoNothing());
  histogram_tester.ExpectTotalCount(kHttpProgIdValidationHistogramName, 1);
  histogram_tester.ExpectBucketCount(kHttpProgIdValidationHistogramName, 0, 1);
}

TEST_F(DefaultBrowserManagerTest,
       DefaultBrowserCheckTrueNegativeWithHTTPAssoc) {
  shell_delegate().set_default_state(shell_integration::NOT_DEFAULT);
  shell_delegate().set_http_assoc_prog_id(u"NotChromeHTML");

  base::HistogramTester histogram_tester;
  default_browser_manager().GetDefaultBrowserState(base::DoNothing());
  histogram_tester.ExpectTotalCount(kHttpProgIdValidationHistogramName, 1);
  histogram_tester.ExpectBucketCount(kHttpProgIdValidationHistogramName, 1, 1);
}

TEST_F(DefaultBrowserManagerTest,
       DefaultBrowserCheckFalsePositiveWithHTTPAssoc) {
  shell_delegate().set_default_state(shell_integration::IS_DEFAULT);
  shell_delegate().set_http_assoc_prog_id(u"NotChromeHTML");

  base::HistogramTester histogram_tester;
  default_browser_manager().GetDefaultBrowserState(base::DoNothing());
  histogram_tester.ExpectTotalCount(kHttpProgIdValidationHistogramName, 1);
  histogram_tester.ExpectBucketCount(kHttpProgIdValidationHistogramName, 2, 1);
}

TEST_F(DefaultBrowserManagerTest,
       DefaultBrowserCheckFalseNegativeWithHTTPAssoc) {
  shell_delegate().set_default_state(shell_integration::NOT_DEFAULT);
  shell_delegate().set_http_assoc_prog_id(u"ChromeHTML");

  base::HistogramTester histogram_tester;
  default_browser_manager().GetDefaultBrowserState(base::DoNothing());
  histogram_tester.ExpectTotalCount(kHttpProgIdValidationHistogramName, 1);
  histogram_tester.ExpectBucketCount(kHttpProgIdValidationHistogramName, 3, 1);
}

TEST_F(DefaultBrowserManagerTest,
       DefaultBrowserCheckNotValidatedForNonDefinitiveResult) {
  shell_delegate().set_default_state(shell_integration::OTHER_MODE_IS_DEFAULT);
  shell_delegate().set_http_assoc_prog_id(u"ChromeHTML");

  base::HistogramTester histogram_tester;
  default_browser_manager().GetDefaultBrowserState(base::DoNothing());
  histogram_tester.ExpectTotalCount(kHttpProgIdValidationHistogramName, 0);
}

constexpr std::string_view kHttpProgIdRegistryValidationHistogramName =
    "DefaultBrowser.HttpProgIdRegistryValidationResult";

TEST_F(DefaultBrowserManagerTest, DefaultBrowserCheckTruePositiveWithRegistry) {
  shell_delegate().set_default_state(shell_integration::IS_DEFAULT);
  CreateDefaultBrowserKey(L"ChromeHTML");

  base::HistogramTester histogram_tester;
  base::StatisticsRecorder::HistogramWaiter waiter(
      kHttpProgIdRegistryValidationHistogramName);
  default_browser_manager().GetDefaultBrowserState(base::DoNothing());
  waiter.Wait();
  histogram_tester.ExpectTotalCount(kHttpProgIdRegistryValidationHistogramName,
                                    1);
  histogram_tester.ExpectBucketCount(kHttpProgIdRegistryValidationHistogramName,
                                     0, 1);
}

TEST_F(DefaultBrowserManagerTest, DefaultBrowserCheckTrueNegativeWithRegistry) {
  shell_delegate().set_default_state(shell_integration::NOT_DEFAULT);
  CreateDefaultBrowserKey(L"NotChromeHTML");

  base::HistogramTester histogram_tester;
  base::StatisticsRecorder::HistogramWaiter waiter(
      kHttpProgIdRegistryValidationHistogramName);
  default_browser_manager().GetDefaultBrowserState(base::DoNothing());
  waiter.Wait();
  histogram_tester.ExpectTotalCount(kHttpProgIdRegistryValidationHistogramName,
                                    1);
  histogram_tester.ExpectBucketCount(kHttpProgIdRegistryValidationHistogramName,
                                     1, 1);
}

TEST_F(DefaultBrowserManagerTest,
       DefaultBrowserCheckFalsePositiveWithRegistry) {
  shell_delegate().set_default_state(shell_integration::IS_DEFAULT);
  CreateDefaultBrowserKey(L"NotChromeHTML");

  base::HistogramTester histogram_tester;
  base::StatisticsRecorder::HistogramWaiter waiter(
      kHttpProgIdRegistryValidationHistogramName);
  default_browser_manager().GetDefaultBrowserState(base::DoNothing());
  waiter.Wait();
  histogram_tester.ExpectTotalCount(kHttpProgIdRegistryValidationHistogramName,
                                    1);
  histogram_tester.ExpectBucketCount(kHttpProgIdRegistryValidationHistogramName,
                                     2, 1);
}

TEST_F(DefaultBrowserManagerTest,
       DefaultBrowserCheckFalseNegativeWithRegistry) {
  shell_delegate().set_default_state(shell_integration::NOT_DEFAULT);
  CreateDefaultBrowserKey(L"ChromeHTML");

  base::HistogramTester histogram_tester;
  base::StatisticsRecorder::HistogramWaiter waiter(
      kHttpProgIdRegistryValidationHistogramName);
  default_browser_manager().GetDefaultBrowserState(base::DoNothing());
  waiter.Wait();
  histogram_tester.ExpectTotalCount(kHttpProgIdRegistryValidationHistogramName,
                                    1);
  histogram_tester.ExpectBucketCount(kHttpProgIdRegistryValidationHistogramName,
                                     3, 1);
}

#endif  // BUILDFLAG(IS_WIN)

}  // namespace default_browser
