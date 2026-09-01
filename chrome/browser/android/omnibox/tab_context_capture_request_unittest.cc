// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/omnibox/tab_context_capture_request.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/ui/contextual_search/tab_contextualization_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "components/lens/contextual_input.h"
#include "components/pdf/common/constants.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::ReturnRef;

namespace {

class MockTabContextualizationController
    : public lens::TabContextualizationController {
 public:
  explicit MockTabContextualizationController(tabs::TabInterface* tab)
      : lens::TabContextualizationController(tab) {}
  ~MockTabContextualizationController() override = default;

  MOCK_METHOD(void,
              GetPageContext,
              (GetPageContextCallback callback),
              (override));
};

}  // namespace

class TabContextCaptureRequestTest : public testing::Test {
 public:
  TabContextCaptureRequestTest() = default;
  ~TabContextCaptureRequestTest() override = default;

  void SetUp() override {
    // Default to enabled.
    scoped_feature_list_.InitAndEnableFeature(
        chrome::android::kOnDemandBackgroundTabContextCapture);

    // This WebContents isn't actually loaded.
    web_contents_ = web_contents_factory_.CreateWebContents(&profile_);
  }

  content::WebContents* web_contents() { return web_contents_; }
  content::BrowserTaskEnvironment* task_environment() {
    return &task_environment_;
  }

  void CreateTabContextCaptureRequest(
      base::OnceCallback<void(std::unique_ptr<lens::ContextualInputData>)>
          callback) {
    mock_tab_interface_ = std::make_unique<tabs::MockTabInterface>();
    tab_weak_factory_ =
        std::make_unique<base::WeakPtrFactory<tabs::TabInterface>>(
            mock_tab_interface_.get());
    EXPECT_CALL(*mock_tab_interface_, GetContents())
        .WillRepeatedly(Return(web_contents()));
    EXPECT_CALL(*mock_tab_interface_, GetWeakPtr())
        .WillRepeatedly(Return(tab_weak_factory_->GetWeakPtr()));
    EXPECT_CALL(*mock_tab_interface_, GetUnownedUserDataHost())
        .WillRepeatedly(ReturnRef(unowned_user_data_host_));

    mock_controller_ = std::make_unique<MockTabContextualizationController>(
        mock_tab_interface_.get());

    // TabContextCaptureRequest manages its own lifetime.
    request_ = new TabContextCaptureRequest(
        mock_controller_.get(), mock_tab_interface_.get(), std::move(callback));
  }

  void TearDown() override { mock_controller_.reset(); }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfile profile_;
  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<content::WebContents> web_contents_;

  std::unique_ptr<tabs::MockTabInterface> mock_tab_interface_;
  std::unique_ptr<MockTabContextualizationController> mock_controller_;
  base::test::ScopedFeatureList scoped_feature_list_;
  ui::UnownedUserDataHost unowned_user_data_host_;
  // Raw pointer to the request, for observing. It deletes itself.
  raw_ptr<TabContextCaptureRequest> request_ = nullptr;

  std::unique_ptr<base::WeakPtrFactory<tabs::TabInterface>> tab_weak_factory_;
};

TEST_F(TabContextCaptureRequestTest, CaptureTriggeredWhenPageLoaded) {
  base::RunLoop run_loop;
  CreateTabContextCaptureRequest(base::BindLambdaForTesting(
      [&](std::unique_ptr<lens::ContextualInputData> data) {
        run_loop.Quit();
      }));

  EXPECT_CALL(*mock_controller_, GetPageContext(_))
      .Times(1)
      .WillOnce([](auto callback) {
        std::move(callback).Run(std::make_unique<lens::ContextualInputData>());
      });

  // Simulate Load Completed.
  request_->Start();
  request_->DocumentOnLoadCompletedInPrimaryMainFrame();

  task_environment()->FastForwardBy(base::Seconds(5));

  run_loop.Run();
}

TEST_F(TabContextCaptureRequestTest, CaptureTriggeredAfterTimeout) {
  base::RunLoop run_loop;
  CreateTabContextCaptureRequest(base::BindLambdaForTesting(
      [&](std::unique_ptr<lens::ContextualInputData> data) {
        run_loop.Quit();
      }));

  EXPECT_CALL(*mock_controller_, GetPageContext(_))
      .Times(1)
      .WillOnce([](auto callback) {
        std::move(callback).Run(std::make_unique<lens::ContextualInputData>());
      });

  // Simulate no signals and timeout.
  request_->Start();
  task_environment()->FastForwardBy(base::Seconds(30));

  run_loop.Run();
}

TEST_F(TabContextCaptureRequestTest, CallbackRunGenericWhenDestroyed) {
  base::RunLoop run_loop;
  CreateTabContextCaptureRequest(base::BindLambdaForTesting(
      [&](std::unique_ptr<lens::ContextualInputData> data) {
        EXPECT_EQ(data, nullptr);
        run_loop.Quit();
      }));

  // Simulate WebContents destroyed.
  request_->Start();
  request_->WebContentsDestroyed();

  run_loop.Run();
}

TEST_F(TabContextCaptureRequestTest, CaptureTriggeredImmediately) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndDisableFeature(
      chrome::android::kOnDemandBackgroundTabContextCapture);

  base::RunLoop run_loop;
  CreateTabContextCaptureRequest(base::BindLambdaForTesting(
      [&](std::unique_ptr<lens::ContextualInputData> data) {
        run_loop.Quit();
      }));

  EXPECT_CALL(*mock_controller_, GetPageContext(_))
      .Times(1)
      .WillOnce([](auto callback) {
        std::move(callback).Run(std::make_unique<lens::ContextualInputData>());
      });

  // Simulate immediate capture.
  request_->Start();

  run_loop.Run();
}

TEST_F(TabContextCaptureRequestTest,
       CaptureTriggeredAfterTimeoutWithOptimization) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/
      {chrome::android::kOnDemandBackgroundTabContextCapture,
       chrome::android::kOnDemandBackgroundTabContextCaptureOptimization},
      /*disabled_features=*/{});

  base::RunLoop run_loop;
  CreateTabContextCaptureRequest(base::BindLambdaForTesting(
      [&](std::unique_ptr<lens::ContextualInputData> data) {
        run_loop.Quit();
      }));

  EXPECT_CALL(*mock_controller_, GetPageContext(_))
      .Times(1)
      .WillOnce([](auto callback) {
        std::move(callback).Run(std::make_unique<lens::ContextualInputData>());
      });

  request_->Start();
  // Fast forward by 24 seconds (should not trigger yet since timeout is 25s).
  task_environment()->FastForwardBy(base::Seconds(24));

  // Fast forward remaining 1 second (total 25 seconds).
  task_environment()->FastForwardBy(base::Seconds(1));
  run_loop.Run();
}

TEST_F(TabContextCaptureRequestTest,
       CaptureTriggeredAfterCustomFallbackTimeout) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{chrome::android::kOnDemandBackgroundTabContextCapture, {}},
       {chrome::android::kOnDemandBackgroundTabContextCaptureOptimization,
        {{"initial_fallback_delay_seconds", "15"}}}},
      /*disabled_features=*/{});

  base::RunLoop run_loop;
  CreateTabContextCaptureRequest(base::BindLambdaForTesting(
      [&](std::unique_ptr<lens::ContextualInputData> data) {
        run_loop.Quit();
      }));

  EXPECT_CALL(*mock_controller_, GetPageContext(_))
      .Times(1)
      .WillOnce([](auto callback) {
        std::move(callback).Run(std::make_unique<lens::ContextualInputData>());
      });

  request_->Start();
  task_environment()->FastForwardBy(base::Seconds(14));

  task_environment()->FastForwardBy(base::Seconds(1));
  run_loop.Run();
}

TEST_F(TabContextCaptureRequestTest, CaptureAbortedWhenOverallTimeoutExpires) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/
      {chrome::android::kOnDemandBackgroundTabContextCapture,
       chrome::android::kOnDemandBackgroundTabContextCaptureOptimization},
      /*disabled_features=*/{});

  base::RunLoop run_loop;
  CreateTabContextCaptureRequest(base::BindLambdaForTesting(
      [&](std::unique_ptr<lens::ContextualInputData> data) {
        EXPECT_EQ(data, nullptr);
        run_loop.Quit();
      }));

  // Simulate GetPageContext hanging (e.g. APC or screenshot hanging).
  EXPECT_CALL(*mock_controller_, GetPageContext(_)).Times(1);

  request_->Start();
  // Fallback triggers GetPageContext at 25s.
  task_environment()->FastForwardBy(base::Seconds(25));
  EXPECT_FALSE(run_loop.AnyQuitCalled());

  // Fast forward to 34s (1 second before 35s overall timeout).
  task_environment()->FastForwardBy(base::Seconds(9));
  EXPECT_FALSE(run_loop.AnyQuitCalled());

  // At 35s, the umbrella timeout fires and aborts via UnableToCapture().
  task_environment()->FastForwardBy(base::Seconds(1));
  run_loop.Run();
}

TEST_F(TabContextCaptureRequestTest,
       CaptureAbortedWhenCustomOverallTimeoutExpires) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{chrome::android::kOnDemandBackgroundTabContextCapture, {}},
       {chrome::android::kOnDemandBackgroundTabContextCaptureOptimization,
        {{"overall_timeout_seconds", "20"}}}},
      /*disabled_features=*/{});

  base::RunLoop run_loop;
  CreateTabContextCaptureRequest(base::BindLambdaForTesting(
      [&](std::unique_ptr<lens::ContextualInputData> data) {
        EXPECT_EQ(data, nullptr);
        run_loop.Quit();
      }));

  // GetPageContext should not be called because overall timeout (20s)
  // fires before the fallback delay (25s).
  EXPECT_CALL(*mock_controller_, GetPageContext(_)).Times(0);

  request_->Start();
  task_environment()->FastForwardBy(base::Seconds(19));
  EXPECT_FALSE(run_loop.AnyQuitCalled());

  task_environment()->FastForwardBy(base::Seconds(1));
  run_loop.Run();
}

class TestTabContextualizationController
    : public lens::TabContextualizationController {
 public:
  explicit TestTabContextualizationController(tabs::TabInterface* tab)
      : lens::TabContextualizationController(tab) {}
  ~TestTabContextualizationController() override = default;

  MOCK_METHOD(void,
              FetchPageContextInternal,
              (GetPageContextCallback callback),
              (override));
};

class TabContextualizationControllerTest : public testing::Test {
 public:
  TabContextualizationControllerTest() = default;
  ~TabContextualizationControllerTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        chrome::android::kOnDemandBackgroundTabContextCaptureOptimization);
    web_contents_ = web_contents_factory_.CreateWebContents(&profile_);
    mock_tab_interface_ = std::make_unique<tabs::MockTabInterface>();
    tab_weak_factory_ =
        std::make_unique<base::WeakPtrFactory<tabs::TabInterface>>(
            mock_tab_interface_.get());
    EXPECT_CALL(*mock_tab_interface_, GetContents())
        .WillRepeatedly(Return(web_contents()));
    EXPECT_CALL(*mock_tab_interface_, GetWeakPtr())
        .WillRepeatedly(Return(tab_weak_factory_->GetWeakPtr()));
    EXPECT_CALL(*mock_tab_interface_, GetUnownedUserDataHost())
        .WillRepeatedly(ReturnRef(unowned_user_data_host_));

    controller_ = std::make_unique<TestTabContextualizationController>(
        mock_tab_interface_.get());
  }

  void TearDown() override { controller_.reset(); }

  content::WebContents* web_contents() { return web_contents_; }
  content::BrowserTaskEnvironment* task_environment() {
    return &task_environment_;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfile profile_;
  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<content::WebContents> web_contents_;

  std::unique_ptr<tabs::MockTabInterface> mock_tab_interface_;
  std::unique_ptr<TestTabContextualizationController> controller_;
  base::test::ScopedFeatureList scoped_feature_list_;
  ui::UnownedUserDataHost unowned_user_data_host_;
  std::unique_ptr<base::WeakPtrFactory<tabs::TabInterface>> tab_weak_factory_;
};

TEST_F(TabContextualizationControllerTest, FlushTriggeredByOverallTimeout) {
  content::WebContentsTester::For(web_contents())->TestSetIsLoading(true);

  base::RunLoop run_loop;
  EXPECT_CALL(*controller_, FetchPageContextInternal(_))
      .Times(1)
      .WillOnce([&](auto callback) {
        std::move(callback).Run(std::make_unique<lens::ContextualInputData>());
        run_loop.Quit();
      });

  controller_->GetPageContext(base::DoNothing());

  // At 4 seconds (before the 5-second default timeout),
  // FetchPageContextInternal should not have been called yet.
  task_environment()->FastForwardBy(base::Seconds(4));

  // At 5 seconds, the overall flush timer fires and triggers
  // FetchPageContextInternal.
  task_environment()->FastForwardBy(base::Seconds(1));
  run_loop.Run();
}

TEST_F(TabContextualizationControllerTest,
       DidFinishLoadFlushesAndCancelsTimer) {
  content::WebContentsTester::For(web_contents())->TestSetIsLoading(true);

  EXPECT_CALL(*controller_, FetchPageContextInternal(_)).Times(1);

  controller_->GetPageContext(base::DoNothing());

  // Simulate DidFinishLoad on the primary main frame.
  content::RenderFrameHost* main_rfh = web_contents()->GetPrimaryMainFrame();
  static_cast<content::WebContentsObserver*>(controller_.get())
      ->DidFinishLoad(main_rfh, GURL("https://example.com"));

  // Fast forward past the 5-second timer. FetchPageContextInternal should not
  // be called a second time.
  task_environment()->FastForwardBy(base::Seconds(10));
}

TEST_F(TabContextualizationControllerTest, OverallTimeoutRespectedFromParam) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{chrome::android::kOnDemandBackgroundTabContextCaptureOptimization,
        {{"overall_flush_timeout_seconds", "3"}}}},
      /*disabled_features=*/{});

  content::WebContentsTester::For(web_contents())->TestSetIsLoading(true);

  base::RunLoop run_loop;
  EXPECT_CALL(*controller_, FetchPageContextInternal(_))
      .Times(1)
      .WillOnce([&](auto callback) {
        std::move(callback).Run(std::make_unique<lens::ContextualInputData>());
        run_loop.Quit();
      });

  controller_->GetPageContext(base::DoNothing());

  task_environment()->FastForwardBy(base::Seconds(2));
  task_environment()->FastForwardBy(base::Seconds(1));
  run_loop.Run();
}
