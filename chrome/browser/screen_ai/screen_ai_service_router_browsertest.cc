// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/screen_ai/screen_ai_service_router.h"
#include "chrome/browser/screen_ai/screen_ai_service_router_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/services/screen_ai/buildflags/buildflags.h"
#include "content/public/test/browser_test.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_features.mojom-features.h"

namespace {

enum class Params {
  kOCREnabled = 0,
  kMainContentExtractionEnabled = 1,
  kLibraryAvailable = 2,  // This must be the last one as the library is not
                          // available in all trybots.
};

// Library is now only available on Linux.
// TODO(crbug.com/1443346): Expand when more platforms are covered.
#if BUILDFLAG(IS_LINUX) && !defined(MEMORY_SANITIZER)
#define MAX_PARAM_VALUE 8
#else
#define MAX_PARAM_VALUE 4
#endif

class ResultsWaiter {
 public:
  ResultsWaiter() = default;
  ~ResultsWaiter() = default;

  void OnResultsCallback(bool successful) {
    result_ = successful;
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

  void WaitForResult() {
    if (result_) {
      return;
    }

    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void ResetResult() { result_ = absl::nullopt; }

  bool GetResult() {
    EXPECT_TRUE(result_);
    return *result_;
  }

  base::WeakPtr<ResultsWaiter> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 protected:
  std::optional<bool> result_;
  base::OnceClosure quit_closure_;

  base::WeakPtrFactory<ResultsWaiter> weak_ptr_factory_{this};
};

}  // namespace

namespace screen_ai {

class ScreenAIServiceRouterTest : public InProcessBrowserTest,
                                  public ::testing::WithParamInterface<int> {
 public:
  ScreenAIServiceRouterTest() {
    std::vector<base::test::FeatureRef> enabled;
    std::vector<base::test::FeatureRef> disabled;

    if (IsEnabled(Params::kOCREnabled)) {
      enabled.push_back(ax::mojom::features::kScreenAIOCREnabled);
    } else {
      disabled.push_back(ax::mojom::features::kScreenAIOCREnabled);
    }

    if (IsEnabled(Params::kMainContentExtractionEnabled)) {
      enabled.push_back(
          ax::mojom::features::kScreenAIMainContentExtractionEnabled);
    } else {
      disabled.push_back(
          ax::mojom::features::kScreenAIMainContentExtractionEnabled);
    }

    if (IsEnabled(Params::kLibraryAvailable)) {
      enabled.push_back(::features::kScreenAITestMode);
    }

    feature_list_.InitWithFeatures(enabled, disabled);
    VLOG(0) << "Test Setup:\n\tOCR: " << IsEnabled(Params::kOCREnabled)
            << "\n\tMainContentExtraction: "
            << IsEnabled(Params::kMainContentExtractionEnabled)
            << "\n\tLibrary: " << IsEnabled(Params::kLibraryAvailable);
  }

  ~ScreenAIServiceRouterTest() override = default;

  bool IsEnabled(Params param) {
    return (GetParam() & (1 << static_cast<int>(param))) != 0;
  }

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    if (IsEnabled(Params::kLibraryAvailable)) {
      ScreenAIInstallState::GetInstance()->SetComponentFolderForTesting();
    }
  }

  void GetServiceStateAndWaitForResult(ScreenAIServiceRouter::Service service) {
    waiter_.ResetResult();
    router()->GetServiceStateAsync(
        service, base::BindOnce(&ResultsWaiter::OnResultsCallback,
                                waiter_.GetWeakPtr()));
    TriggerDownloadFailIfNeeded();
    waiter_.WaitForResult();
  }

  void TriggerDownloadFailIfNeeded() {
    if (!IsEnabled(Params::kLibraryAvailable)) {
      ScreenAIInstallState::GetInstance()->SetState(
          ScreenAIInstallState::State::kDownloadFailed);
    }
  }

  bool ExpectedInitializationResult(ScreenAIServiceRouter::Service service) {
    if (!IsEnabled(Params::kLibraryAvailable)) {
      return false;
    }

    switch (service) {
      case ScreenAIServiceRouter::Service::kOCR:
        return IsEnabled(Params::kOCREnabled);

      case ScreenAIServiceRouter::Service::kMainContentExtraction:
        return IsEnabled(Params::kMainContentExtractionEnabled);
    }
  }

  ScreenAIServiceRouter* router() {
    return ScreenAIServiceRouterFactory::GetForBrowserContext(
        browser()->profile());
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  ResultsWaiter waiter_;
};

IN_PROC_BROWSER_TEST_P(ScreenAIServiceRouterTest, OCRInitialization) {
  ScreenAIServiceRouter::Service service = ScreenAIServiceRouter::Service::kOCR;

  GetServiceStateAndWaitForResult(service);

  bool expected_result = ExpectedInitializationResult(service);
  EXPECT_EQ(waiter_.GetResult(), expected_result);
  EXPECT_EQ(router()->IsConnectionBoundForTesting(service), expected_result);

  // What about the second time?
  GetServiceStateAndWaitForResult(service);
  EXPECT_EQ(waiter_.GetResult(), expected_result);
}

IN_PROC_BROWSER_TEST_P(ScreenAIServiceRouterTest,
                       MainContentExtractionInitialization) {
  ScreenAIServiceRouter::Service service =
      ScreenAIServiceRouter::Service::kMainContentExtraction;

  GetServiceStateAndWaitForResult(service);

  bool expected_result = ExpectedInitializationResult(service);
  EXPECT_EQ(waiter_.GetResult(), expected_result);
  EXPECT_EQ(router()->IsConnectionBoundForTesting(service), expected_result);

  // What about the second time?
  GetServiceStateAndWaitForResult(service);
  EXPECT_EQ(waiter_.GetResult(), expected_result);
}

IN_PROC_BROWSER_TEST_P(ScreenAIServiceRouterTest, MixedInitialization) {
  ScreenAIServiceRouter::Service service =
      ScreenAIServiceRouter::Service::kMainContentExtraction;
  GetServiceStateAndWaitForResult(service);
  EXPECT_EQ(waiter_.GetResult(), ExpectedInitializationResult(service));

  service = ScreenAIServiceRouter::Service::kOCR;
  GetServiceStateAndWaitForResult(service);
  EXPECT_EQ(waiter_.GetResult(), ExpectedInitializationResult(service));
}

// Tests if asking for initialization of a second service before getting the
// result of the first one passes.
IN_PROC_BROWSER_TEST_P(ScreenAIServiceRouterTest,
                       MixedInitializationWithoutWait) {
  ScreenAIServiceRouter::Service service1 =
      ScreenAIServiceRouter::Service::kOCR;
  ScreenAIServiceRouter::Service service2 =
      ScreenAIServiceRouter::Service::kMainContentExtraction;

  ResultsWaiter waiter1;
  ResultsWaiter waiter2;

  router()->GetServiceStateAsync(
      service1,
      base::BindOnce(&ResultsWaiter::OnResultsCallback, waiter1.GetWeakPtr()));
  router()->GetServiceStateAsync(
      service2,
      base::BindOnce(&ResultsWaiter::OnResultsCallback, waiter2.GetWeakPtr()));

  TriggerDownloadFailIfNeeded();

  waiter1.WaitForResult();
  waiter2.WaitForResult();

  EXPECT_EQ(waiter1.GetResult(), ExpectedInitializationResult(service1));
  EXPECT_EQ(waiter2.GetResult(), ExpectedInitializationResult(service2));
}

// Tests if repeated asking for initialization of a service before getting the
// result of the first request passes.
IN_PROC_BROWSER_TEST_P(ScreenAIServiceRouterTest,
                       RepeatedInitializationWithoutWait) {
  ScreenAIServiceRouter::Service service = ScreenAIServiceRouter::Service::kOCR;

  ResultsWaiter waiter1;
  ResultsWaiter waiter2;

  router()->GetServiceStateAsync(
      service,
      base::BindOnce(&ResultsWaiter::OnResultsCallback, waiter1.GetWeakPtr()));
  router()->GetServiceStateAsync(
      service,
      base::BindOnce(&ResultsWaiter::OnResultsCallback, waiter2.GetWeakPtr()));

  TriggerDownloadFailIfNeeded();

  waiter1.WaitForResult();
  waiter2.WaitForResult();

  EXPECT_EQ(waiter1.GetResult(), ExpectedInitializationResult(service));
  EXPECT_EQ(waiter2.GetResult(), ExpectedInitializationResult(service));
}

INSTANTIATE_TEST_SUITE_P(All,
                         ScreenAIServiceRouterTest,
                         ::testing::Range(0, MAX_PARAM_VALUE));

// TODO(crbug.com/1520424): Consider adding a test that sets the download state
// to false, and then sets the test download path only when download request
// arrives.

}  // namespace screen_ai
