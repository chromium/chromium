// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dev_ui/android/dev_ui_loader_throttle.h"

#include <deque>
#include <memory>
#include <utility>

#include "chrome/android/modules/dev_ui/provider/dev_ui_module_provider.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/mock_navigation_handle.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace dev_ui {

namespace {

const char* const kNonDevUiUrls[] = {
    "https://example.com",
    "https://example.com/path?query#frag",
    "chrome://credits",
    "chrome://credits/path?query#frag",
};

const char* const kDevUiUrls[] = {
    "chrome://bluetooth-internals",
    "chrome://bluetooth-internals/path?query#frag",
};

/******** MockDevUiModuleProvider ********/

// Mock DevUiModuleProvider that overrides the main module install / load
// functions, and provides SimulateAsyncInstall().
class MockDevUiModuleProvider : public DevUiModuleProvider {
 public:
  MockDevUiModuleProvider() = default;
  ~MockDevUiModuleProvider() override = default;

  // DevUiModuleProvider:
  bool ModuleInstalled() override { return is_installed_; }

  // DevUiModuleProvider:
  void InstallModule(base::OnceCallback<void(bool)> on_complete) override {
    on_complete_queue_.emplace_back(std::move(on_complete));
  }

  // DevUiModuleProvider:
  void EnsureLoaded() override { is_loaded_ = true; }

  void Reset() {
    is_installed_ = false;
    is_loaded_ = false;
    on_complete_queue_.clear();
  }

  // Simulate DevUI DFN install, and dispatches all on-complete callbacks.
  void SimulateAsyncInstall(bool install_succeeds) {
    is_installed_ = is_installed_ || install_succeeds;
    while (!on_complete_queue_.empty()) {
      std::move(on_complete_queue_.front()).Run(install_succeeds);
      on_complete_queue_.pop_front();
    }
  }

  // Direct access of fake installation states.
  void SetIsInstalled(bool is_installed) { is_installed_ = is_installed; }
  bool GetIsInstalled() const { return is_installed_; }
  void SetIsLoaded(bool is_loaded) { is_loaded_ = is_loaded; }
  bool GetIsLoaded() const { return is_loaded_; }

 private:
  bool is_installed_ = false;
  bool is_loaded_ = false;
  std::deque<base::OnceCallback<void(bool)>> on_complete_queue_;
};

/******** TestDevUiLoaderThrottle ********/

// DevUiLoaderThrottle that overrides and captures Resume() and
// CancelDeferredNavigation(). This is needed because:
// * In actual usage, both functions can lead to throttle deletion, which is
//   problematic since the tests own the throttle instance.
// * We want to record results for verification.
// Note that this is instantiated directly, instead of using
// DevUiLoaderThrottle::MaybeCreateThrottleFor().
class TestDevUiLoaderThrottle : public DevUiLoaderThrottle {
 public:
  explicit TestDevUiLoaderThrottle(content::NavigationHandle* navigation_handle)
      : DevUiLoaderThrottle(navigation_handle), cancel_result(LAST) {}
  ~TestDevUiLoaderThrottle() override = default;
  TestDevUiLoaderThrottle(const TestDevUiLoaderThrottle&) = delete;
  const TestDevUiLoaderThrottle& operator=(const TestDevUiLoaderThrottle&) =
      delete;

  // content::NavigationThrottle:
  void Resume() override { called_resume = true; }

  // content::NavigationThrottle:
  void CancelDeferredNavigation(
      content::NavigationThrottle::ThrottleCheckResult result) override {
    called_cancel = true;
    cancel_result = std::move(result);
  }

  bool called_resume = false;
  bool called_cancel = false;
  content::NavigationThrottle::ThrottleCheckResult cancel_result;
};

/******** DevUiLoaderThrottleTest ********/

class DevUiLoaderThrottleTest : public ChromeRenderViewHostTestHarness {
 public:
  DevUiLoaderThrottleTest() = default;
  ~DevUiLoaderThrottleTest() override = default;
  DevUiLoaderThrottleTest(const DevUiLoaderThrottleTest&) = delete;
  const DevUiLoaderThrottleTest& operator=(const DevUiLoaderThrottleTest&) =
      delete;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    DevUiModuleProvider::SetTestInstance(&mock_provider_);
  }

  void TearDown() override {
    mock_provider_.Reset();
    DevUiModuleProvider::SetTestInstance(nullptr);
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  MockDevUiModuleProvider mock_provider_;
};

class DevUiLoaderThrottleFencedFrameTest : public DevUiLoaderThrottleTest {
 public:
  DevUiLoaderThrottleFencedFrameTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kFencedFrames, {{"implementation_type", "mparch"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

TEST_F(DevUiLoaderThrottleTest, ShouldInstallDevUiDfm) {
  auto ShouldInstallDevUiDfm = DevUiLoaderThrottle::ShouldInstallDevUiDfm;
  for (const char* url_string : kNonDevUiUrls)
    EXPECT_FALSE(ShouldInstallDevUiDfm(GURL(url_string)));
  for (const char* url_string : kDevUiUrls)
    EXPECT_TRUE(ShouldInstallDevUiDfm(GURL(url_string)));
}

// Test to ensure that pages that are purposefully left in the base module are
// not accidentally moved to the DevUI DFM.
TEST_F(DevUiLoaderThrottleTest, PreventAccidentalInclusion) {
  auto ShouldInstallDevUiDfm = DevUiLoaderThrottle::ShouldInstallDevUiDfm;

  // Useful to have the catalog always.
  EXPECT_FALSE(ShouldInstallDevUiDfm(GURL("chrome://chrome-urls")));
  EXPECT_FALSE(ShouldInstallDevUiDfm(GURL("chrome://about")));
  // Inclusion in base module is mandatory.
  EXPECT_FALSE(ShouldInstallDevUiDfm(GURL("chrome://credits")));
  // Well-loved game, and shown when there's no internet (cannot install DFM).
  EXPECT_FALSE(ShouldInstallDevUiDfm(GURL("chrome://dino")));
  // chrome://flags has relatively high usage.
  EXPECT_FALSE(ShouldInstallDevUiDfm(GURL("chrome://flags")));
  // Useful for filing bugs.
  EXPECT_FALSE(ShouldInstallDevUiDfm(GURL("chrome://version")));
  // Used by Android WebView.
  EXPECT_FALSE(ShouldInstallDevUiDfm(GURL("chrome://safe-browsing")));
}

TEST_F(DevUiLoaderThrottleTest, MaybeCreateThrottleFor) {
  bool is_installed = false;
  auto creates_throttle = [&](const std::string& url_string) -> bool {
    mock_provider_.Reset();
    mock_provider_.SetIsInstalled(is_installed);
    content::MockNavigationHandle handle(GURL(url_string), main_rfh());
    std::unique_ptr<content::NavigationThrottle> throttle =
        DevUiLoaderThrottle::MaybeCreateThrottleFor(&handle);
    return throttle != nullptr;
  };

  // Case 1: DevUI DFM is not installed.
  is_installed = false;
  // Case 1a: Non DevUI URLs: Throttle not created.
  for (const char* url_string : kNonDevUiUrls) {
    EXPECT_FALSE(creates_throttle(url_string));
    EXPECT_FALSE(mock_provider_.GetIsLoaded());
  }
  // Case 1b: DevUI URLs: Throttle created, but no load takes place.
  for (const char* url_string : kDevUiUrls) {
    EXPECT_TRUE(creates_throttle(url_string));
    EXPECT_FALSE(mock_provider_.GetIsLoaded());
  }

  // Case 2: DevUI DFM is installed.
  is_installed = true;
  // Case 2a: Non DevUI URLs: Throttle not created.
  for (const char* url_string : kNonDevUiUrls) {
    EXPECT_FALSE(creates_throttle(url_string));
    EXPECT_FALSE(mock_provider_.GetIsLoaded());
  }
  // Case 2b: DevUI URLs: Throttle not created, but load takes place.
  for (const char* url_string : kDevUiUrls) {
    EXPECT_FALSE(creates_throttle(url_string));
    EXPECT_TRUE(mock_provider_.GetIsLoaded());
  }
}

TEST_F(DevUiLoaderThrottleFencedFrameTest, MaybeCreateThrottleFor) {
  bool is_installed = false;
  content::RenderFrameHost* render_frame_host = nullptr;
  auto creates_throttle = [&](const std::string& url_string) -> bool {
    mock_provider_.Reset();
    mock_provider_.SetIsInstalled(is_installed);
    content::MockNavigationHandle handle(GURL(url_string), render_frame_host);
    std::unique_ptr<content::NavigationThrottle> throttle =
        DevUiLoaderThrottle::MaybeCreateThrottleFor(&handle);
    return throttle != nullptr;
  };

  content::RenderFrameHostTester::For(main_rfh())
      ->InitializeRenderFrameIfNeeded();
  render_frame_host =
      content::RenderFrameHostTester::For(main_rfh())->AppendFencedFrame();
  EXPECT_TRUE(render_frame_host);

  // In any case, throttles should not be created in fenced frames.
  is_installed = false;
  for (const char* url_string : kNonDevUiUrls) {
    EXPECT_FALSE(creates_throttle(url_string));
    EXPECT_FALSE(mock_provider_.GetIsLoaded());
  }
  for (const char* url_string : kDevUiUrls) {
    EXPECT_FALSE(creates_throttle(url_string));
    EXPECT_FALSE(mock_provider_.GetIsLoaded());
  }
  is_installed = true;
  for (const char* url_string : kNonDevUiUrls) {
    EXPECT_FALSE(creates_throttle(url_string));
    EXPECT_FALSE(mock_provider_.GetIsLoaded());
  }
  for (const char* url_string : kDevUiUrls) {
    EXPECT_FALSE(creates_throttle(url_string));
    EXPECT_FALSE(mock_provider_.GetIsLoaded());
  }
}

TEST_F(DevUiLoaderThrottleTest, InstallSuccess) {
  for (const char* url_string : kDevUiUrls) {
    mock_provider_.Reset();
    content::MockNavigationHandle handle(GURL(url_string), main_rfh());
    auto throttle = std::make_unique<TestDevUiLoaderThrottle>(&handle);
    EXPECT_FALSE(throttle->called_resume);
    EXPECT_FALSE(throttle->called_cancel);
    EXPECT_FALSE(mock_provider_.GetIsInstalled());
    EXPECT_FALSE(mock_provider_.GetIsLoaded());

    // Simulate request start.
    EXPECT_EQ(content::NavigationThrottle::DEFER, throttle->WillStartRequest());
    EXPECT_FALSE(throttle->called_resume);
    EXPECT_FALSE(throttle->called_cancel);
    EXPECT_FALSE(mock_provider_.GetIsInstalled());
    EXPECT_FALSE(mock_provider_.GetIsLoaded());

    // Simulate actual install (success)
    mock_provider_.SimulateAsyncInstall(true);  // !
    EXPECT_TRUE(throttle->called_resume);
    EXPECT_FALSE(throttle->called_cancel);
    EXPECT_TRUE(mock_provider_.GetIsInstalled());
    EXPECT_TRUE(mock_provider_.GetIsLoaded());
  }
}

TEST_F(DevUiLoaderThrottleTest, InstallQueued) {
  mock_provider_.Reset();
  // Page 1 request.
  content::MockNavigationHandle handle1(GURL(kDevUiUrls[0]), main_rfh());
  auto throttle1 = std::make_unique<TestDevUiLoaderThrottle>(&handle1);
  // Page 2 request.
  content::MockNavigationHandle handle2(GURL(kDevUiUrls[1]), main_rfh());
  auto throttle2 = std::make_unique<TestDevUiLoaderThrottle>(&handle2);
  EXPECT_FALSE(mock_provider_.GetIsInstalled());
  EXPECT_FALSE(mock_provider_.GetIsLoaded());

  // Simulate request start of page 1. This triggers install.
  EXPECT_EQ(content::NavigationThrottle::DEFER, throttle1->WillStartRequest());

  // Simulate request start of page 2. This does not trigger install. However,
  // the on-complete callback is queued, and will be called.
  EXPECT_EQ(content::NavigationThrottle::DEFER, throttle2->WillStartRequest());

  // Simulate actual install.
  mock_provider_.SimulateAsyncInstall(true);  // !
  EXPECT_TRUE(throttle1->called_resume);
  EXPECT_FALSE(throttle1->called_cancel);
  EXPECT_TRUE(throttle2->called_resume);
  EXPECT_FALSE(throttle2->called_cancel);
  EXPECT_TRUE(mock_provider_.GetIsInstalled());
  EXPECT_TRUE(mock_provider_.GetIsLoaded());
}

TEST_F(DevUiLoaderThrottleTest, InstallRedundant) {
  mock_provider_.Reset();
  // Page 1 request.
  content::MockNavigationHandle handle1(GURL(kDevUiUrls[0]), main_rfh());
  auto throttle1 = std::make_unique<TestDevUiLoaderThrottle>(&handle1);
  // Page 2 request.
  content::MockNavigationHandle handle2(GURL(kDevUiUrls[1]), main_rfh());
  auto throttle2 = std::make_unique<TestDevUiLoaderThrottle>(&handle2);
  EXPECT_FALSE(mock_provider_.GetIsInstalled());
  EXPECT_FALSE(mock_provider_.GetIsLoaded());

  // Simulate request start of page 1.
  EXPECT_EQ(content::NavigationThrottle::DEFER, throttle1->WillStartRequest());

  // Simulate actual install.
  mock_provider_.SimulateAsyncInstall(true);  // !
  EXPECT_TRUE(throttle1->called_resume);
  EXPECT_FALSE(throttle1->called_cancel);
  EXPECT_TRUE(mock_provider_.GetIsInstalled());
  EXPECT_TRUE(mock_provider_.GetIsLoaded());

  // Simulate request start of page 2. The request was made before install
  // completes, but request start occurs after install completes. In this case,
  // simply proceed.
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle2->WillStartRequest());
  EXPECT_FALSE(throttle2->called_resume);
  EXPECT_FALSE(throttle2->called_cancel);
  EXPECT_TRUE(mock_provider_.GetIsInstalled());
  EXPECT_TRUE(mock_provider_.GetIsLoaded());
}

TEST_F(DevUiLoaderThrottleTest, InstallFailure) {
  for (const char* url_string : kDevUiUrls) {
    mock_provider_.Reset();
    content::MockNavigationHandle handle(GURL(url_string), main_rfh());
    auto throttle = std::make_unique<TestDevUiLoaderThrottle>(&handle);
    EXPECT_FALSE(throttle->called_resume);
    EXPECT_FALSE(throttle->called_cancel);
    EXPECT_FALSE(mock_provider_.GetIsInstalled());
    EXPECT_FALSE(mock_provider_.GetIsLoaded());

    // Simulate request start.
    EXPECT_EQ(content::NavigationThrottle::DEFER, throttle->WillStartRequest());
    EXPECT_FALSE(throttle->called_resume);
    EXPECT_FALSE(throttle->called_cancel);
    EXPECT_FALSE(mock_provider_.GetIsInstalled());
    EXPECT_FALSE(mock_provider_.GetIsLoaded());

    // Simulate actual install (failure)
    mock_provider_.SimulateAsyncInstall(false);  // !
    EXPECT_FALSE(throttle->called_resume);
    EXPECT_TRUE(throttle->called_cancel);
    EXPECT_FALSE(mock_provider_.GetIsInstalled());
    EXPECT_FALSE(mock_provider_.GetIsLoaded());
    EXPECT_EQ(content::NavigationThrottle::CANCEL,
              throttle->cancel_result.action());
    EXPECT_EQ(net::ERR_CONNECTION_FAILED,
              throttle->cancel_result.net_error_code());
    EXPECT_TRUE(throttle->cancel_result.error_page_content().has_value());
    std::string page_content =
        throttle->cancel_result.error_page_content().value();
    EXPECT_NE(std::string::npos, page_content.find("<!DOCTYPE html"));
  }
}

}  // namespace dev_ui
