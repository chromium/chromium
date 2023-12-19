// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_view.h"

#include <optional>

#include "ash/picker/model/picker_search_results.h"
#include "ash/picker/views/picker_search_field_view.h"
#include "ash/picker/views/picker_search_results_view.h"
#include "ash/picker/views/picker_user_education_view.h"
#include "ash/picker/views/picker_zero_state_view.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_ash_web_view.h"
#include "ash/test/test_ash_web_view_factory.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/background.h"
#include "ui/views/view_utils.h"

namespace ash {
namespace {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Optional;
using ::testing::Property;
using ::testing::Truly;

using PickerViewTest = AshTestBase;

class FakePickerViewDelegate : public PickerView::Delegate {
 public:
  using FakeSearchFunction =
      base::RepeatingCallback<PickerSearchResults(std::u16string_view query)>;

  FakePickerViewDelegate()
      : search_function_(base::BindRepeating(
            [](std::u16string_view query) { return PickerSearchResults(); })) {}
  explicit FakePickerViewDelegate(FakeSearchFunction search_function)
      : search_function_(search_function) {}

  std::unique_ptr<AshWebView> CreateWebView(
      const AshWebView::InitParams& params) override {
    return ash_web_view_factory_.Create(params);
  }

  void StartSearch(const std::u16string& query,
                   SearchResultsCallback callback) override {
    callback.Run(search_function_.Run(query));
  }

  void InsertResult(const PickerSearchResult& result) override {
    last_inserted_result_ = result;
  }

  bool ShouldPaint() override { return true; }

  std::optional<PickerSearchResult> last_inserted_result() const {
    return last_inserted_result_;
  }

 private:
  TestAshWebViewFactory ash_web_view_factory_;
  FakeSearchFunction search_function_;
  std::optional<PickerSearchResult> last_inserted_result_;
};

PickerView* GetPickerViewFromWidget(views::Widget& widget) {
  return views::AsViewClass<PickerView>(
      widget.non_client_view()->client_view()->children().front());
}

TEST_F(PickerViewTest, CreateWidgetHasCorrectHierarchy) {
  auto widget =
      PickerView::CreateWidget(std::make_unique<FakePickerViewDelegate>());

  // Widget should contain a NonClientView, which has a NonClientFrameView for
  // borders and shadows, and a ClientView with a sole child of the PickerView.
  ASSERT_TRUE(widget);
  ASSERT_TRUE(widget->non_client_view());
  ASSERT_TRUE(widget->non_client_view()->frame_view());
  ASSERT_TRUE(widget->non_client_view()->client_view());
  EXPECT_THAT(widget->non_client_view()->client_view()->children(),
              ElementsAre(Truly(views::IsViewClass<PickerView>)));
}

TEST_F(PickerViewTest, CreateWidgetHasCorrectBorder) {
  auto widget =
      PickerView::CreateWidget(std::make_unique<FakePickerViewDelegate>());

  EXPECT_TRUE(widget->non_client_view()->frame_view()->GetBorder());
}

TEST_F(PickerViewTest, BackgroundIsCorrect) {
  auto widget =
      PickerView::CreateWidget(std::make_unique<FakePickerViewDelegate>());
  PickerView* view = GetPickerViewFromWidget(*widget);

  ASSERT_TRUE(view);
  ASSERT_TRUE(view->background());
  EXPECT_EQ(
      view->background()->get_color(),
      view->GetColorProvider()->GetColor(cros_tokens::kCrosSysBaseElevated));
}

TEST_F(PickerViewTest, SizeIsCorrect) {
  auto widget =
      PickerView::CreateWidget(std::make_unique<FakePickerViewDelegate>());
  widget->Show();
  PickerView* view = GetPickerViewFromWidget(*widget);

  EXPECT_EQ(view->size(), gfx::Size(420, 480));
}

TEST_F(PickerViewTest, ShowsZeroStateView) {
  auto widget =
      PickerView::CreateWidget(std::make_unique<FakePickerViewDelegate>());
  PickerView* view = GetPickerViewFromWidget(*widget);

  EXPECT_THAT(view->search_field_view_for_testing(),
              Property(&views::View::GetVisible, true));
  EXPECT_THAT(view->zero_state_view_for_testing(),
              Property(&views::View::GetVisible, true));
  EXPECT_THAT(view->search_results_view_for_testing(),
              Property(&views::View::GetVisible, false));
}

TEST_F(PickerViewTest, NonEmptySearchFieldContentsSwitchesToSearchResultsView) {
  auto widget =
      PickerView::CreateWidget(std::make_unique<FakePickerViewDelegate>());
  widget->Show();
  PickerView* view = GetPickerViewFromWidget(*widget);

  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);

  EXPECT_THAT(view->zero_state_view_for_testing(),
              Property(&views::View::GetVisible, false));
  EXPECT_THAT(view->search_results_view_for_testing(),
              Property(&views::View::GetVisible, true));
}

TEST_F(PickerViewTest, EmptySearchFieldContentsSwitchesToZeroStateView) {
  auto widget =
      PickerView::CreateWidget(std::make_unique<FakePickerViewDelegate>());
  widget->Show();
  PickerView* view = GetPickerViewFromWidget(*widget);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);

  PressAndReleaseKey(ui::KeyboardCode::VKEY_BACK, ui::EF_NONE);

  EXPECT_THAT(view->zero_state_view_for_testing(),
              Property(&views::View::GetVisible, true));
  EXPECT_THAT(view->search_results_view_for_testing(),
              Property(&views::View::GetVisible, false));
}

TEST_F(PickerViewTest, LeftClickSearchResultSelectsResult) {
  base::test::TestFuture<void> future;
  // TODO(b/317111483): Change the delegate a raw pointer to make this less
  // awkward.
  auto delegate = std::make_unique<FakePickerViewDelegate>(
      base::BindLambdaForTesting([&](std::u16string_view query) {
        future.SetValue();
        return PickerSearchResults({{
            PickerSearchResults::Section(u"section",
                                         {{PickerSearchResult(u"result")}}),
        }});
      }));
  auto* delegate_ptr = delegate.get();
  auto widget = PickerView::CreateWidget(std::move(delegate));
  widget->Show();
  PickerView* view = GetPickerViewFromWidget(*widget);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(future.Wait());

  // TODO(b/316935667): Actually click on a result item instead of the whole
  // view.
  LeftClickOn(&view->search_results_view_for_testing());

  EXPECT_THAT(delegate_ptr->last_inserted_result(),
              Optional(Property(&PickerSearchResult::text, Eq(u"result"))));
}

}  // namespace
}  // namespace ash
