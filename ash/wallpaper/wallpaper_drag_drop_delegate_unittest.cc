// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_drag_drop_delegate.h"

#include <memory>
#include <set>

#include "ash/drag_drop/drag_drop_controller.h"
#include "ash/drag_drop/draggable_test_view.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// Aliases.
using testing::Invoke;
using testing::Return;
using testing::StrictMock;
using testing::WithArg;

// MockWallpaperDragDropDelegate -----------------------------------------------

// A mock implementation of `WallpaperDragDropDelegate` for testing.
class MockWallpaperDragDropDelegate : public WallpaperDragDropDelegate {
 public:
  // WallpaperDragDropDelegate:
  MOCK_METHOD(void,
              GetDropFormats,
              (int* formats, std::set<ui::ClipboardFormatType>* types),
              (override));

  MOCK_METHOD(bool, CanDrop, (const ui::OSExchangeData& data), (override));

  MOCK_METHOD(void,
              OnDragEntered,
              (const ui::OSExchangeData& data,
               const gfx::Point& location_in_screen),
              (override));

  MOCK_METHOD(ui::DragDropTypes::DragOperation,
              OnDragUpdated,
              (const ui::OSExchangeData& data,
               const gfx::Point& location_in_screen),
              (override));

  MOCK_METHOD(void, OnDragExited, (), (override));

  MOCK_METHOD(ui::mojom::DragOperation,
              OnDrop,
              (const ui::OSExchangeData& data,
               const gfx::Point& location_in_screen),
              (override));
};

}  // namespace

// WallpaperDragDropDelegateTest -----------------------------------------------

enum class Scenario {
  kWithoutDelegate,
  kWithDelegateAndInterestingPayload,
  kWithDelegateAndInterestingButRejectedPayload,
  kWithDelegateAndUninterestingPayload,
};

// Base class for tests of `WallpaperDragDropDelegate`, parameterized by testing
// scenario and whether or not to drop data over the wallpaper.
class WallpaperDragDropDelegateTest
    : public AshTestBase,
      public testing::WithParamInterface<
          std::tuple<Scenario, /*drop_over_wallpaper=*/bool>> {
 public:
  // Returns the `delegate_` for drag-and-drop events over the wallpaper.
  // NOTE: May be `nullptr` depending on test parameterization.
  StrictMock<MockWallpaperDragDropDelegate>* delegate() { return delegate_; }

  // Returns whether data should be dropped over the wallpaper given test
  // parameterization.
  bool drop_over_wallpaper() const { return std::get<1>(GetParam()); }

  // Returns the testing scenario given parameterization.
  Scenario scenario() const { return std::get<0>(GetParam()); }

  // Returns the `widget_` from which data can be drag-and-dropped.
  views::Widget* widget() { return widget_.get(); }

  // Moves the mouse to the center of the specified `widget`.
  void MoveMouseTo(views::Widget* widget) {
    GetEventGenerator()->MoveMouseTo(
        widget->GetWindowBoundsInScreen().CenterPoint(), /*count=*/10);
  }

  // Moves the mouse by the specified `x` and `y` offsets.
  void MoveMouseBy(int x, int y) {
    auto* event_generator = GetEventGenerator();
    event_generator->MoveMouseTo(
        event_generator->current_screen_location() + gfx::Vector2d(x, y),
        /*count=*/10);
  }

  // Performs a press/release of the left mouse button.
  void PressLeftButton() { GetEventGenerator()->PressLeftButton(); }
  void ReleaseLeftButton() { GetEventGenerator()->ReleaseLeftButton(); }

  // Verifies that any previously set expectations of the drag-and-drop
  // `delegate_` have been fulfilled, and resets expectations to their initial
  // state. No-ops if `delegate_` does not exist.
  void VerifyAndResetExpectations() {
    if (!delegate_) {
      return;
    }

    testing::Mock::VerifyAndClearExpectations(delegate_);

    ON_CALL(*delegate_, CanDrop)
        .WillByDefault(testing::Return(
            scenario() !=
            Scenario::kWithDelegateAndInterestingButRejectedPayload));

    ON_CALL(*delegate_, GetDropFormats)
        .WillByDefault(testing::Invoke(
            [&](int* formats, std::set<ui::ClipboardFormatType>* types) {
              if (scenario() !=
                  Scenario::kWithDelegateAndUninterestingPayload) {
                *formats |= OSExchangeData::Format::STRING;
              }
            }));

    ON_CALL(*delegate_, OnDragUpdated)
        .WillByDefault(
            testing::Return(ui::DragDropTypes::DragOperation::DRAG_COPY));
  }

 private:
  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    // Prevent blocking during drag-and-drop sequences.
    ShellTestApi().drag_drop_controller()->set_should_block_during_drag_drop(
        false);

    // Create and show a `widget_` from which data can be drag-and-dropped.
    widget_ = CreateFramelessTestWidget();
    widget_->SetContentsView(std::make_unique<DraggableTestView>());
    widget_->CenterWindow(gfx::Size(100, 100));
    widget_->Show();

    if (scenario() == Scenario::kWithoutDelegate) {
      return;
    }

    // Initialize and set default expectations for the `delegate_` for
    // drag-and-drop events over the wallpaper.
    auto delegate =
        std::make_unique<StrictMock<MockWallpaperDragDropDelegate>>();
    delegate_ = delegate.get();
    VerifyAndResetExpectations();
    WallpaperController::Get()->SetDragDropDelegate(std::move(delegate));
  }

  void TearDown() override {
    widget_->CloseNow();
    widget_.reset();

    if (delegate_) {
      WallpaperController::Get()->SetDragDropDelegate(nullptr);
      delegate_ = nullptr;
    }

    AshTestBase::TearDown();
  }

  // The delegate, owned by the `WallpaperControllerImpl`, for drag-and-drop
  // events over the wallpaper. May be `nullptr` depending on test
  // parameterization.
  raw_ptr<StrictMock<MockWallpaperDragDropDelegate>, ExperimentalAsh>
      delegate_ = nullptr;

  // The widget from which data can be drag-and-dropped.
  std::unique_ptr<views::Widget> widget_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    WallpaperDragDropDelegateTest,
    testing::Combine(
        testing::Values(Scenario::kWithoutDelegate,
                        Scenario::kWithDelegateAndInterestingPayload,
                        Scenario::kWithDelegateAndInterestingButRejectedPayload,
                        Scenario::kWithDelegateAndUninterestingPayload),
        /*drop_over_wallpaper=*/testing::Bool()),
    [](const auto& info) {
      std::string scenario;
      switch (/*scenario=*/std::get<0>(info.param)) {
        case Scenario::kWithoutDelegate:
          scenario = "WithoutDelegate";
          break;
        case Scenario::kWithDelegateAndInterestingPayload:
          scenario = "WithDelegateAndInterestingPayload";
          break;
        case Scenario::kWithDelegateAndInterestingButRejectedPayload:
          scenario = "WithDelegateAndInterestingButRejectedPayload";
          break;
        case Scenario::kWithDelegateAndUninterestingPayload:
          scenario = "WithDelegateAndUninterestingPayload";
          break;
      }
      return base::StrCat({/*drop_over_wallpaper=*/std::get<1>(info.param)
                               ? "OverWallpaper"
                               : "OverWidget",
                           scenario});
    });

// Tests -----------------------------------------------------------------------

// Verifies that drag-and-drop events propagate as expected given the presence/
// absence of a delegate and various parameterized testing scenarios.
TEST_P(WallpaperDragDropDelegateTest, DragAndDrop) {
  // Initiate a drag from `widget()`.
  MoveMouseTo(widget());
  PressLeftButton();

  // The data will be dragged from `widget()` over the wallpaper. The sequence
  // of expected events depends on the presence/absence of the `delegate()` and
  // the parameterized testing `scenario()`.
  if (delegate()) {
    testing::InSequence sequence;
    EXPECT_CALL(*delegate(), GetDropFormats);
    if (scenario() != Scenario::kWithDelegateAndUninterestingPayload) {
      EXPECT_CALL(*delegate(), CanDrop);
      if (scenario() !=
          Scenario::kWithDelegateAndInterestingButRejectedPayload) {
        EXPECT_CALL(*delegate(), OnDragEntered);
        EXPECT_CALL(*delegate(), OnDragUpdated).Times(testing::AtLeast(1));
      }
    }
  }

  // Drag the data from `widget()` over the wallpaper.
  MoveMouseBy(/*x=*/widget()->GetWindowBoundsInScreen().width(), /*y=*/0);
  VerifyAndResetExpectations();

  // The data will be dropped over either the wallpaper or the `widget()`. The
  // sequence of expected events depends on the presence/absence of the
  // `delegate()` and the paramaterized testing `scenario()`.
  if (delegate()) {
    if (scenario() == Scenario::kWithDelegateAndInterestingPayload) {
      if (drop_over_wallpaper()) {
        EXPECT_CALL(*delegate(), OnDrop);
      } else {
        testing::InSequence sequence;
        EXPECT_CALL(*delegate(), OnDragUpdated).Times(testing::AtLeast(1));
        EXPECT_CALL(*delegate(), OnDragExited);
      }
    }
  }

  // If the data should not be dropped over the wallpaper, drag the data back
  // over the `widget()`.
  if (!drop_over_wallpaper()) {
    MoveMouseTo(widget());
  }

  // Drop the drag data over either the wallpaper or the `widget()`.
  ReleaseLeftButton();
  VerifyAndResetExpectations();
}

}  // namespace ash
