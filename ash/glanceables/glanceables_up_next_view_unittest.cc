// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/glanceables_up_next_view.h"

#include "ash/test/ash_test_base.h"
#include "ui/views/controls/label.h"

namespace ash {
namespace {

class GlanceablesUpNextViewTest : public NoSessionAshTestBase {
 public:
  void SetUp() override {
    NoSessionAshTestBase::SetUp();
    up_next_view_ = std::make_unique<GlanceablesUpNextView>();
  }

 protected:
  std::unique_ptr<GlanceablesUpNextView> up_next_view_;
};

TEST_F(GlanceablesUpNextViewTest, RendersCorrectly) {
  // Events list contains rendered event items inside.
  const auto& items = up_next_view_->events_list_items_views_for_test();
  EXPECT_EQ(items.size(), 5u);
  for (const auto& item : items) {
    EXPECT_EQ(std::get<0>(item)->GetText(), u"James / Artsiom");
    EXPECT_EQ(std::get<1>(item)->GetText(), u"2:00 – 2:30pm");
  }
}

}  // namespace
}  // namespace ash
