// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/ui/assistive_accessibility_view.h"

#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/views/test/ax_event_counter.h"

namespace ui {
namespace ime {

class AssistiveAccessiblityViewTest : public ChromeViewsTestBase {
 public:
  AssistiveAccessiblityViewTest() {}
  ~AssistiveAccessiblityViewTest() override {}

 protected:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    assistive_accessibility_view_ =
        new AssistiveAccessibilityView(GetContext());
  }

  void TearDown() override {
    assistive_accessibility_view_->GetWidget()->CloseNow();
    ChromeViewsTestBase::TearDown();
  }

  AssistiveAccessibilityView* assistive_accessibility_view_;
};

TEST_F(AssistiveAccessiblityViewTest, MakesAnnouncement) {
  views::test::AXEventCounter counter(views::AXEventManager::Get());
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kLiveRegionChanged));
  assistive_accessibility_view_->Announce(u"test");
  task_environment()->FastForwardBy(base::Milliseconds(200));
  EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kLiveRegionChanged));
}

}  // namespace ime
}  // namespace ui
