// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/default_browser_manager.h"

#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/default_browser/default_browser_controller.h"
#include "chrome/browser/default_browser/default_browser_features.h"
#include "chrome/browser/shell_integration.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace default_browser {

class FakeShellDelegate : public DefaultBrowserManager::ShellDelegate {
 public:
  FakeShellDelegate() = default;
  ~FakeShellDelegate() override = default;

  void StartCheckIsDefault(
      shell_integration::DefaultWebClientWorkerCallback callback) override {
    std::move(callback).Run(default_state_);
  }

#if BUILDFLAG(IS_WIN)
  void StartCheckDefaultClientProgId(
      const std::string& scheme,
      base::OnceCallback<void(const std::u16string&)> callback) override {
    std::u16string prog_id = u"";
    if (scheme == "http") {
      prog_id = http_assoc_prog_id_;
    }
    std::move(callback).Run(prog_id);
  }
#endif  // BUILDFLAG(IS_WIN)

  void set_default_state(DefaultBrowserState state) { default_state_ = state; }
  void set_http_assoc_prog_id(const std::u16string& prog_id) {
    http_assoc_prog_id_ = prog_id;
  }

 private:
  DefaultBrowserState default_state_ = shell_integration::NUM_DEFAULT_STATES;
  std::u16string http_assoc_prog_id_ = u"";
};

class DefaultBrowserManagerTest : public testing::Test {
 protected:
  DefaultBrowserManagerTest() {
    scoped_feature_list_.InitAndEnableFeature(
        kPerformDefaultBrowserCheckValidations);

    auto shell_delegate = std::make_unique<FakeShellDelegate>();
    shell_delegate_ = shell_delegate.get();
    default_browser_manager_ =
        std::make_unique<DefaultBrowserManager>(std::move(shell_delegate));
  }

  ~DefaultBrowserManagerTest() override = default;

  DefaultBrowserManager& default_browser_manager() {
    return *default_browser_manager_.get();
  }

  FakeShellDelegate& shell_delegate() { return *shell_delegate_.get(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<DefaultBrowserManager> default_browser_manager_;
  raw_ptr<FakeShellDelegate> shell_delegate_;
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

#endif  // BUILDFLAG(IS_WIN)

}  // namespace default_browser
