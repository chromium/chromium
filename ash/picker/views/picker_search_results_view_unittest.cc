// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_search_results_view.h"

#include "ash/picker/model/picker_search_results.h"
#include "ash/test/ash_test_base.h"
#include "base/test/test_future.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

using ::testing::Eq;
using ::testing::Property;

using PickerSearchResultsViewTest = AshTestBase;

TEST_F(PickerSearchResultsViewTest, LeftClickSelectsSearchResult) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();
  widget->SetFullscreen(true);
  base::test::TestFuture<const PickerSearchResult&> future;
  auto* view = widget->SetContentsView(
      std::make_unique<PickerSearchResultsView>(future.GetCallback()));
  view->SetSearchResults(PickerSearchResults({{
      PickerSearchResults::Section(u"section",
                                   {{PickerSearchResult(u"result")}}),
  }}));

  // TODO(b/316935667): Actually click on a result item instead of the whole
  // view.
  LeftClickOn(view);

  EXPECT_THAT(future.Get(), Property(&PickerSearchResult::text, Eq(u"result")));
}

}  // namespace
}  // namespace ash
