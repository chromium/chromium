// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_search_field_view.h"

#include <string>

#include "ash/picker/metrics/picker_performance_metrics.h"
#include "ash/picker/picker_test_util.h"
#include "ash/picker/views/picker_key_event_handler.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

namespace ash {
namespace {

using PickerSearchFieldViewTest = views::ViewsTestBase;

TEST_F(PickerSearchFieldViewTest, DoesNotTriggerSearchOnConstruction) {
  base::test::TestFuture<const std::u16string&> future;
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  PickerSearchFieldView view(future.GetRepeatingCallback(), &key_event_handler,
                             &metrics);

  EXPECT_FALSE(future.IsReady());
}

TEST_F(PickerSearchFieldViewTest, TriggersSearchOnContentsChange) {
  std::unique_ptr<views::Widget> widget = CreateTestWidget();
  base::test::TestFuture<const std::u16string&> future;
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  auto* view = widget->SetContentsView(std::make_unique<PickerSearchFieldView>(
      future.GetRepeatingCallback(), &key_event_handler, &metrics));

  view->RequestFocus();
  PressAndReleaseKey(*widget, ui::KeyboardCode::VKEY_A);

  EXPECT_EQ(future.Get(), u"a");
}

TEST_F(PickerSearchFieldViewTest, SetPlaceholderText) {
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  PickerSearchFieldView view(base::DoNothing(), &key_event_handler, &metrics);

  view.SetPlaceholderText(u"hello");

  EXPECT_EQ(view.textfield_for_testing().GetPlaceholderText(), u"hello");
}

}  // namespace
}  // namespace ash
