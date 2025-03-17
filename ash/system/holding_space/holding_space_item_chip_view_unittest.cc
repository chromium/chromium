// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_item_chip_view.h"

#include <memory>
#include <optional>
#include <string>

#include "ash/public/cpp/holding_space/holding_space_file.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/system/holding_space/holding_space_ash_test_base.h"
#include "ash/system/holding_space/holding_space_view_delegate.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// Aliases.
using Type = HoldingSpaceItem::Type;

// Helpers ---------------------------------------------------------------------

void FlushMessageLoop() {
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace

// HoldingSpaceItemChipViewTest ------------------------------------------------

class HoldingSpaceItemChipViewTest : public HoldingSpaceAshTestBase {
 public:
  const HoldingSpaceItem* item() const { return item_; }
  const HoldingSpaceItemChipView* view() const { return view_; }

 private:
  // HoldingSpaceAshTestBase:
  void SetUp() override {
    HoldingSpaceAshTestBase::SetUp();

    // Initialize widget.
    widget_ = std::make_unique<views::Widget>();
    widget_->Init(views::Widget::InitParams{
        views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET});

    // Initialize view and dependencies.
    view_delegate_ = std::make_unique<HoldingSpaceViewDelegate>(nullptr);
    item_ = AddItem(Type::kDownload, base::FilePath("file_path"));
    view_ = widget_->GetRootView()->AddChildView(
        std::make_unique<HoldingSpaceItemChipView>(view_delegate_.get(),
                                                   item_));
  }

  void TearDown() override {
    item_ = nullptr;
    view_ = nullptr;
    widget_->CloseNow();
    widget_.reset();
    view_delegate_.reset();

    HoldingSpaceAshTestBase::TearDown();
  }

  raw_ptr<HoldingSpaceItem> item_ = nullptr;
  raw_ptr<HoldingSpaceItemChipView> view_ = nullptr;
  std::unique_ptr<HoldingSpaceViewDelegate> view_delegate_;
  views::UniqueWidgetPtr widget_;
};

// Tests -----------------------------------------------------------------------

// Verifies that tooltip text is updated appropriately.
TEST_F(HoldingSpaceItemChipViewTest, TooltipText) {
  constexpr gfx::Point p;

  // Case: Empty primary tooltip, empty secondary tooltip.
  EXPECT_TRUE(view()->GetRenderedTooltipText(p).empty());

  // Populate secondary text.
  // NOTE: Text must be long enough to force a tooltip in the underlying label.
  model()->UpdateItem(item()->id())->SetSecondaryText(u"secondary secondary");
  FlushMessageLoop();

  // Case: Empty primary tooltip, populated secondary tooltip.
  EXPECT_EQ(view()->GetRenderedTooltipText(p),
            u"file_path, secondary secondary");

  // Populate primary text.
  // NOTE: Text must be long enough to force a tooltip in the underlying label.
  model()->UpdateItem(item()->id())->SetText(u"primary primary");
  FlushMessageLoop();

  // Case: Populated primary tooltip, populated secondary tooltip.
  EXPECT_EQ(view()->GetRenderedTooltipText(p),
            u"primary primary, secondary secondary");

  // Clear secondary text.
  model()->UpdateItem(item()->id())->SetSecondaryText(std::nullopt);
  FlushMessageLoop();

  // Case: Populated primary tooltip, empty secondary tooltip.
  EXPECT_EQ(view()->GetRenderedTooltipText(p), u"primary primary");
}

}  // namespace ash
