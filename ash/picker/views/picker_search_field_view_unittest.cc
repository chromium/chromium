// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_search_field_view.h"

#include <string>

#include "ash/test/ash_test_base.h"
#include "base/test/test_future.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

using PickerSearchFieldViewTest = AshTestBase;

TEST_F(PickerSearchFieldViewTest, TriggersSearchOnConstruction) {
  base::test::TestFuture<const std::u16string&> future;
  PickerSearchFieldView view(future.GetRepeatingCallback());

  EXPECT_EQ(future.Get(), u"");
}

TEST_F(PickerSearchFieldViewTest, TriggersSearchOnContentsChange) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();
  base::test::TestFuture<const std::u16string&> future;
  auto* view = widget->SetContentsView(
      std::make_unique<PickerSearchFieldView>(future.GetRepeatingCallback()));
  future.Clear();

  view->RequestFocus();
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);

  EXPECT_EQ(future.Get(), u"a");
}

TEST_F(PickerSearchFieldViewTest, SetPlaceholderText) {
  PickerSearchFieldView view(base::DoNothing());

  view.SetPlaceholderText(u"hello");

  EXPECT_EQ(view.textfield_for_testing().GetPlaceholderText(), u"hello");
}

}  // namespace
}  // namespace ash
