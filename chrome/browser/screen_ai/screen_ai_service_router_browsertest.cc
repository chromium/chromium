// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/screen_ai/screen_ai_service_router.h"

#include <optional>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/screen_ai/screen_ai_service_router_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "services/screen_ai/buildflags/buildflags.h"
#include "services/screen_ai/public/cpp/utilities.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_features.mojom-features.h"

namespace {

enum class LibraryAvailablity {
  kAvailable,
  kAvailableWithDelay,
  kNotAvailable,
};

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

  void ResetResult() { result_ = std::nullopt; }

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

base::FilePath GetComponentBinaryPath() {
#if BUILDFLAG(ENABLE_SCREEN_AI_BROWSERTESTS)
  return screen_ai::GetComponentBinaryPathForTests();
#else
  NOTREACHED() << "Test library is used on a not-suppported platform.";
#endif
}

}  // namespace

namespace screen_ai {

class ScreenAIServiceRouterTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<
          std::tuple<LibraryAvailablity, bool, bool>> {
 public:
  ScreenAIServiceRouterTest() {
    std::vector<base::test::FeatureRef> enabled;
    std::vector<base::test::FeatureRef> disabled;

    if (IsOCREnabled()) {
      enabled.push_back(ax::mojom::features::kScreenAIOCREnabled);
    } else {
      disabled.push_back(ax::mojom::features::kScreenAIOCREnabled);
    }

    if (IsMainContentExtractionEnabled()) {
      enabled.push_back(
          ax::mojom::features::kScreenAIMainContentExtractionEnabled);
    } else {
      disabled.push_back(
          ax::mojom::features::kScreenAIMainContentExtractionEnabled);
    }

    if (IsLibraryAvailableAtStartUp() || IsLibraryAvailableLater()) {
      enabled.push_back(::features::kScreenAITestMode);
    }

    feature_list_.InitWithFeatures(enabled, disabled);
  }

  ~ScreenAIServiceRouterTest() override = default;

  bool IsLibraryAvailableAtStartUp() {
#if BUILDFLAG(ENABLE_SCREEN_AI_BROWSERTESTS)
    return std::get<0>(GetParam()) == LibraryAvailablity::kAvailable;
#else
    return false;
#endif
  }

  bool IsLibraryAvailableLater() {
#if BUILDFLAG(ENABLE_SCREEN_AI_BROWSERTESTS)
    return std::get<0>(GetParam()) == LibraryAvailablity::kAvailableWithDelay;
#else
    return false;
#endif
  }

  bool IsOCREnabled() { return std::get<1>(GetParam()); }
  bool IsMainContentExtractionEnabled() { return std::get<2>(GetParam()); }

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    if (IsLibraryAvailableAtStartUp()) {
      base::FilePath library_path = GetComponentBinaryPath();
      CHECK(!library_path.empty());
      CHECK(base::PathExists(library_path));
      ScreenAIInstallState::GetInstance()->SetComponentFolder(
          library_path.DirName());
    } else if (IsLibraryAvailableLater()) {
      // Assume library download has failed before, but will succeed once it's
      // asked again.
      ScreenAIInstallState::GetInstance()->SetState(
          ScreenAIInstallState::State::kDownloadFailed);
    }
  }

  void GetServiceStateAndWaitForResult(ScreenAIServiceRouter::Service service) {
    waiter_.ResetResult();
    router()->GetServiceStateAsync(
        service, base::BindOnce(&ResultsWaiter::OnResultsCallback,
                                waiter_.GetWeakPtr()));
    SimulateDownload();
    waiter_.WaitForResult();
  }

  void SimulateDownload() {
    if (IsLibraryAvailableAtStartUp()) {
      return;
    }

    if (IsLibraryAvailableLater()) {
      if (!ScreenAIInstallState::GetInstance()->IsComponentAvailable()) {
        base::ThreadPool::PostTaskAndReplyWithResult(
            FROM_HERE,
            {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
            base::BindOnce(&GetComponentBinaryPath),
            base::BindOnce([](const base::FilePath component_path) {
              ScreenAIInstallState::GetInstance()->SetComponentFolder(
                  component_path.DirName());
            }));
      }

      return;
    }

    ScreenAIInstallState::GetInstance()->SetState(
        ScreenAIInstallState::State::kDownloadFailed);
  }

  bool ExpectedInitializationResult(ScreenAIServiceRouter::Service service) {
    if (!IsLibraryAvailableAtStartUp() && !IsLibraryAvailableLater()) {
      return false;
    }

    switch (service) {
      case ScreenAIServiceRouter::Service::kOCR:
        return IsOCREnabled();

      case ScreenAIServiceRouter::Service::kMainContentExtraction:
        return IsMainContentExtractionEnabled();
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

  SimulateDownload();

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

  SimulateDownload();

  waiter1.WaitForResult();
  waiter2.WaitForResult();

  EXPECT_EQ(waiter1.GetResult(), ExpectedInitializationResult(service));
  EXPECT_EQ(waiter2.GetResult(), ExpectedInitializationResult(service));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ScreenAIServiceRouterTest,
    ::testing::Combine(testing::Values(LibraryAvailablity::kAvailable,
                                       LibraryAvailablity::kAvailableWithDelay,
                                       LibraryAvailablity::kNotAvailable),
                       testing::Bool(),
                       testing::Bool()),
    [](const testing::TestParamInfo<std::tuple<LibraryAvailablity, bool, bool>>&
           info) {
      return base::StringPrintf(
          "Library_%s_OCR_%s_MCE_%s",
          std::get<0>(info.param) == LibraryAvailablity::kAvailable
              ? "Available"
              : (std::get<0>(info.param) ==
                         LibraryAvailablity::kAvailableWithDelay
                     ? "AvailableWithDelay"
                     : "NotAvailable"),
          std::get<1>(info.param) ? "Enabled" : "Disabled",
          std::get<2>(info.param) ? "Enabled" : "Disabled");
    });

}  // namespace screen_ai
