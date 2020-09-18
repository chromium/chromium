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

constexpr char kLongText[] =
    "123456789101112131415161718192021222324252627282930313233343536373839404"
    "\r\n\t1424344454647484950";
constexpr char kLongAnswer[] =
    "123456789101112131415161718192021222324252627282930313233343536373839404"
    "1424344454647484950";
constexpr char kTruncatedLongText[] =
    "123456789101112131415161718192021222324252627282930313233343536373839…";

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
//// accesses resources.
class QuickAnswersMenuObserverTest : public InProcessBrowserTest {
 public:
  QuickAnswersMenuObserverTest() {
    feature_list_.InitWithFeatures({chromeos::features::kQuickAnswers},
                                   {chromeos::features::kQuickAnswersRichUi});
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
  void VerifyMenuItems(int index,
                       int expected_command_id,
                       const std::string& expected_title,
                       bool enabled) {
    MockRenderViewContextMenu::MockMenuItem item;
    menu()->GetMenuItem(index, &item);
    EXPECT_EQ(expected_command_id, item.command_id);
    EXPECT_EQ(base::UTF8ToUTF16(expected_title), item.title);
    EXPECT_EQ(enabled, item.enabled);
    EXPECT_FALSE(item.hidden);
  }

  void MockQuickAnswerClient(const std::string expected_query) {
    std::unique_ptr<QuickAnswersRequest> expected_quick_answers_request =
        std::make_unique<QuickAnswersRequest>();
    expected_quick_answers_request->selected_text = expected_query;
    EXPECT_CALL(
        *mock_quick_answers_cient_,
        SendRequest(QuickAnswersRequestEqual(*expected_quick_answers_request)))
        .Times(1);
    observer_->SetQuickAnswerClientForTesting(
        std::move(mock_quick_answers_cient_));
  }

  base::test::ScopedFeatureList feature_list_;

  std::unique_ptr<QuickAnswersMenuObserver> observer_;
  std::unique_ptr<MockQuickAnswersClient> mock_quick_answers_cient_;

  std::unique_ptr<MockRenderViewContextMenu> menu_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(QuickAnswersMenuObserverTest, PlaceHolderMenuItems) {
  MockQuickAnswerClient("sel");
  InitMenu();

  // Shows quick answers loading state.
  ASSERT_EQ(3u, menu()->GetMenuSize());

  // Verify the query menu item.
  VerifyMenuItems(
      /*index=*/0,
      /*command_id=*/IDC_CONTENT_CONTEXT_QUICK_ANSWERS_INLINE_QUERY,
      /*expected_title=*/"sel",
      /*enabled=*/true);
  // Verify the answer menu item.
  VerifyMenuItems(
      /*index=*/1,
      /*command_id=*/IDC_CONTENT_CONTEXT_QUICK_ANSWERS_INLINE_ANSWER,
      /*expected_title=*/"Loading...",
      /*enabled=*/false);
}

IN_PROC_BROWSER_TEST_F(QuickAnswersMenuObserverTest,
                       SanitizeAndTruncateSelectedText) {
  MockQuickAnswerClient(
      "123456789101112131415161718192021222324252627282930313233343536373839404"
      "   1424344454647484950");

  // Init Menu.
  content::ContextMenuParams params;
  static const base::string16 selected_text = base::ASCIIToUTF16(kLongText);
  params.selection_text = selected_text;
  observer_->InitMenu(params);

  // Shows quick answers loading state.
  ASSERT_EQ(3u, menu()->GetMenuSize());

  // Verify the query menu item.
  VerifyMenuItems(
      /*index=*/0,
      /*command_id=*/IDC_CONTENT_CONTEXT_QUICK_ANSWERS_INLINE_QUERY,
      /*expected_title=*/kTruncatedLongText,
      /*enabled=*/true);
  // Verify the answer menu item.
  VerifyMenuItems(
      /*index=*/1,
      /*command_id=*/IDC_CONTENT_CONTEXT_QUICK_ANSWERS_INLINE_ANSWER,
      /*expected_title=*/"Loading...",
      /*enabled=*/false);
}

IN_PROC_BROWSER_TEST_F(QuickAnswersMenuObserverTest, PrimaryAnswerOnly) {
  MockQuickAnswerClient("sel");
  InitMenu();

  std::unique_ptr<QuickAnswer> quick_answer = std::make_unique<QuickAnswer>();
  quick_answer->primary_answer = "primary answer";
  observer_->OnQuickAnswerReceived(std::move(quick_answer));

  // Verify that quick answer menu items is showing.
  ASSERT_EQ(3u, menu()->GetMenuSize());

  // Verify the query menu item.
  VerifyMenuItems(
      /*index=*/0,
      /*command_id=*/IDC_CONTENT_CONTEXT_QUICK_ANSWERS_INLINE_QUERY,
      /*expected_title=*/"sel",
      /*enabled=*/true);

  // Verify the answer menu item.
  VerifyMenuItems(
      /*index=*/1,
      /*command_id=*/IDC_CONTENT_CONTEXT_QUICK_ANSWERS_INLINE_ANSWER,
      /*expected_title=*/"primary answer",
      /*enabled=*/false);
}

IN_PROC_BROWSER_TEST_F(QuickAnswersMenuObserverTest, SecondaryAnswerOnly) {
  MockQuickAnswerClient("sel");
  InitMenu();

  std::unique_ptr<QuickAnswer> quick_answer = std::make_unique<QuickAnswer>();
  quick_answer->secondary_answer = "secondary answer";
  observer_->OnQuickAnswerReceived(std::move(quick_answer));

  // Verify that quick answer menu items is showing.
  ASSERT_EQ(3u, menu()->GetMenuSize());

  // Verify the query menu item.
  VerifyMenuItems(
      /*index=*/0,
      /*command_id=*/IDC_CONTENT_CONTEXT_QUICK_ANSWERS_INLINE_QUERY,
      /*expected_title=*/"secondary answer",
      /*enabled=*/true);

  // Verify the answer menu item.
  VerifyMenuItems(
      /*index=*/1,
      /*command_id=*/IDC_CONTENT_CONTEXT_QUICK_ANSWERS_INLINE_ANSWER,
      /*expected_title=*/"See result in Assistant",
      /*enabled=*/false);
}

IN_PROC_BROWSER_TEST_F(QuickAnswersMenuObserverTest,
                       PrimaryAndSecondaryAnswer) {
  MockQuickAnswerClient("sel");
  InitMenu();

  std::unique_ptr<QuickAnswer> quick_answer = std::make_unique<QuickAnswer>();
  quick_answer->primary_answer = "primary answer";
  quick_answer->secondary_answer = "secondary answer";
  observer_->OnQuickAnswerReceived(std::move(quick_answer));

  // Verify that quick answer menu items is showing.
  ASSERT_EQ(3u, menu()->GetMenuSize());

  // Verify the query menu item.
  VerifyMenuItems(
      /*index=*/0,
      /*command_id=*/IDC_CONTENT_CONTEXT_QUICK_ANSWERS_INLINE_QUERY,
      /*expected_title=*/"secondary answer",
      /*enabled=*/true);

  // Verify the answer menu item.
  VerifyMenuItems(
      /*index=*/1,
      /*command_id=*/IDC_CONTENT_CONTEXT_QUICK_ANSWERS_INLINE_ANSWER,
      /*expected_title=*/"primary answer",
      /*enabled=*/false);
}

IN_PROC_BROWSER_TEST_F(QuickAnswersMenuObserverTest, TruncateLongAnswer) {
  MockQuickAnswerClient("sel");
  InitMenu();

  std::unique_ptr<QuickAnswer> quick_answer = std::make_unique<QuickAnswer>();
  quick_answer->primary_answer = kLongAnswer;
  quick_answer->secondary_answer = kLongAnswer;
  observer_->OnQuickAnswerReceived(std::move(quick_answer));

  // Verify that quick answer menu items is showing.
  ASSERT_EQ(3u, menu()->GetMenuSize());

  // Verify the query menu item.
  VerifyMenuItems(
      /*index=*/0,
      /*command_id=*/IDC_CONTENT_CONTEXT_QUICK_ANSWERS_INLINE_QUERY,
      /*expected_title=*/kTruncatedLongText,
      /*enabled=*/true);

  // Verify the answer menu item.
  VerifyMenuItems(
      /*index=*/1,
      /*command_id=*/IDC_CONTENT_CONTEXT_QUICK_ANSWERS_INLINE_ANSWER,
      /*expected_title=*/kTruncatedLongText,
      /*enabled=*/false);
}

IN_PROC_BROWSER_TEST_F(QuickAnswersMenuObserverTest, NoAnswer) {
  MockQuickAnswerClient("sel");
  InitMenu();

  observer_->OnQuickAnswerReceived(nullptr);

  // Verify that quick answer menu items is showing.
  ASSERT_EQ(3u, menu()->GetMenuSize());

  // Verify the query menu item.
  VerifyMenuItems(
      /*index=*/0,
      /*command_id=*/IDC_CONTENT_CONTEXT_QUICK_ANSWERS_INLINE_QUERY,
      /*expected_title=*/"sel",
      /*enabled=*/true);

  // Verify the answer menu item.
  VerifyMenuItems(
      /*index=*/1,
      /*command_id=*/IDC_CONTENT_CONTEXT_QUICK_ANSWERS_INLINE_ANSWER,
      /*expected_title=*/"See result in Assistant",
      /*enabled=*/false);
}

IN_PROC_BROWSER_TEST_F(QuickAnswersMenuObserverTest, FeatureIneligible) {
  observer_->OnEligibilityChanged(false);

  // Verify that quick answer client is not called to fetch result.
  EXPECT_CALL(*mock_quick_answers_cient_, SendRequest(testing::_)).Times(0);
  observer_->SetQuickAnswerClientForTesting(
      std::move(mock_quick_answers_cient_));

  InitMenu();

  // Verify that no Quick Answer menu items shown.
  ASSERT_EQ(0u, menu()->GetMenuSize());
}

IN_PROC_BROWSER_TEST_F(QuickAnswersMenuObserverTest, NetworkError) {
  MockQuickAnswerClient("sel");
  InitMenu();

  observer_->OnNetworkError();

  // Verify that quick answer menu items is showing.
  ASSERT_EQ(3u, menu()->GetMenuSize());

  // Verify the query menu item.
  VerifyMenuItems(
      /*index=*/0,
      /*command_id=*/IDC_CONTENT_CONTEXT_QUICK_ANSWERS_INLINE_QUERY,
      /*expected_title=*/"sel",
      /*enabled=*/true);

  // Verify the answer menu item.
  VerifyMenuItems(
      /*index=*/1,
      /*command_id=*/IDC_CONTENT_CONTEXT_QUICK_ANSWERS_INLINE_ANSWER,
      /*expected_title=*/"Cannot connect to internet.",
      /*enabled=*/false);
}
