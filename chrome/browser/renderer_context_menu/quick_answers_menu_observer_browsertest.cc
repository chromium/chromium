// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/quick_answers_menu_observer.h"

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/renderer_context_menu/mock_render_view_context_menu.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/components/quick_answers/quick_answers_client.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/components/quick_answers/test/test_helpers.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using chromeos::quick_answers::QuickAnswer;
using chromeos::quick_answers::QuickAnswersClient;
using chromeos::quick_answers::QuickAnswersRequest;

using testing::_;

class MockQuickAnswersClient : public QuickAnswersClient {
 public:
  MockQuickAnswersClient(network::mojom::URLLoaderFactory* url_loader_factory,
                         ash::AssistantState* assistant_state,
                         QuickAnswersMenuObserver* delegate)
      : QuickAnswersClient(url_loader_factory, assistant_state, delegate) {}

  MockQuickAnswersClient(const MockQuickAnswersClient&) = delete;
  MockQuickAnswersClient& operator=(const MockQuickAnswersClient&) = delete;

  // QuickAnswersClient::QuickAnswersClient:
  MOCK_METHOD1(SendRequest, void(const QuickAnswersRequest&));
};

// A test class for Quick Answers. This test should be a browser test because it
// accesses resources.
// TODO(b/171579052): Add test cases for quick answers Rich UI.
class QuickAnswersMenuObserverTest : public InProcessBrowserTest {
 public:
  QuickAnswersMenuObserverTest() {
    feature_list_.InitAndEnableFeature(chromeos::features::kQuickAnswers);
  }

  QuickAnswersMenuObserverTest(const QuickAnswersMenuObserverTest&) = delete;
  QuickAnswersMenuObserverTest& operator=(const QuickAnswersMenuObserverTest&) =
      delete;

  // InProcessBrowserTest overrides:
  void SetUpOnMainThread() override {
    Reset(false);
    mock_quick_answers_cient_ = std::make_unique<MockQuickAnswersClient>(
        /*url_loader_factory=*/nullptr,
        /*assistant_state=*/ash::AssistantState::Get(),
        /*delegate=*/observer_.get());
    observer_->OnEligibilityChanged(true);
  }
  void TearDownOnMainThread() override {
    observer_.reset();
    menu_.reset();
  }

  void Reset(bool incognito) {
    observer_.reset();
    menu_ = std::make_unique<MockRenderViewContextMenu>(incognito);
    observer_ = std::make_unique<QuickAnswersMenuObserver>(menu_.get());
    menu_->SetObserver(observer_.get());
  }

  void InitMenu() {
    content::ContextMenuParams params;
    static const base::string16 selected_text = base::ASCIIToUTF16("sel");
    params.selection_text = selected_text;
    observer_->InitMenu(params);
  }

  MockRenderViewContextMenu* menu() { return menu_.get(); }
  QuickAnswersMenuObserver* observer() { return observer_.get(); }

 protected:
  void MockQuickAnswerClient(const std::string expected_query) {
    std::unique_ptr<QuickAnswersRequest> expected_quick_answers_request =
        std::make_unique<QuickAnswersRequest>();
    expected_quick_answers_request->selected_text = expected_query;
    EXPECT_CALL(
        *mock_quick_answers_cient_,
        SendRequest(QuickAnswersRequestEqual(*expected_quick_answers_request)))
        .Times(1);
  }

  base::test::ScopedFeatureList feature_list_;

  std::unique_ptr<QuickAnswersMenuObserver> observer_;
  std::unique_ptr<MockQuickAnswersClient> mock_quick_answers_cient_;

  std::unique_ptr<MockRenderViewContextMenu> menu_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(QuickAnswersMenuObserverTest, FeatureIneligible) {
  observer_->OnEligibilityChanged(false);

  // Verify that quick answer client is not called to fetch result.
  EXPECT_CALL(*mock_quick_answers_cient_, SendRequest(testing::_)).Times(0);

  InitMenu();

  // Verify that no Quick Answer menu items shown.
  ASSERT_EQ(0u, menu()->GetMenuSize());
}
