// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_category_view.h"

#include <memory>

#include "ash/picker/model/picker_search_results.h"
#include "ash/picker/views/picker_item_view.h"
#include "ash/picker/views/picker_section_view.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/view_drawn_waiter.h"
#include "base/functional/callback_helpers.h"
#include "base/test/test_future.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::SizeIs;

using PickerCategoryViewTest = AshTestBase;

auto MatchesResultSection(const PickerSearchResults::Section& section) {
  return AllOf(
      Property(&PickerSectionView::title_for_testing,
               Property(&views::Label::GetText, Eq(section.heading()))),
      Property(&PickerSectionView::item_views_for_testing,
               SizeIs(section.results().size())));
}

TEST_F(PickerCategoryViewTest, CreatesResultsSections) {
  PickerCategoryView view(base::DoNothing());
  const PickerSearchResults kSearchResults({{
      PickerSearchResults::Section(u"Saved",
                                   {{PickerSearchResult(u"Result A")}}),
      PickerSearchResults::Section(
          u"Recently used",
          {{PickerSearchResult(u"Result B"), PickerSearchResult(u"Result C")}}),
  }});
  view.SetResults(kSearchResults);

  EXPECT_THAT(
      view.section_views_for_testing(),
      ElementsAre(Pointee(MatchesResultSection(kSearchResults.sections()[0])),
                  Pointee(MatchesResultSection(kSearchResults.sections()[1]))));
}

TEST_F(PickerCategoryViewTest, LeftClickSelectsResult) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();
  widget->SetFullscreen(true);
  base::test::TestFuture<const PickerSearchResult&> future;
  auto* view = widget->SetContentsView(
      std::make_unique<PickerCategoryView>(future.GetCallback()));
  view->SetResults(PickerSearchResults({{
      PickerSearchResults::Section(u"Recently used",
                                   {{PickerSearchResult(u"result")}}),
  }}));
  ASSERT_THAT(view->section_views_for_testing(), Not(IsEmpty()));
  ASSERT_THAT(view->section_views_for_testing()[0]->item_views_for_testing(),
              Not(IsEmpty()));

  PickerItemView* result_view =
      view->section_views_for_testing()[0]->item_views_for_testing()[0];
  ViewDrawnWaiter().Wait(result_view);
  LeftClickOn(result_view);

  EXPECT_THAT(future.Get(), Property(&PickerSearchResult::text, Eq(u"result")));
}

}  // namespace
}  // namespace ash
