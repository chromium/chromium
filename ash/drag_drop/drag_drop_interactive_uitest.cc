// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/drag_drop/drag_drop_controller.h"

#include <memory>

#include "ash/drag_drop/draggable_test_view.h"
#include "ash/shell.h"
#include "ash/test/ash_interactive_ui_test_base.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/test/ui_controls.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

class TargetView : public views::View {
 public:
  TargetView() : dropped_(false) {}

  TargetView(const TargetView&) = delete;
  TargetView& operator=(const TargetView&) = delete;

  ~TargetView() override = default;

  // views::View overrides:
  bool GetDropFormats(
      int* formats,
      std::set<ui::ClipboardFormatType>* format_types) override {
    *formats = ui::OSExchangeData::STRING;
    return true;
  }
  bool AreDropTypesRequired() override { return false; }
  bool CanDrop(const OSExchangeData& data) override { return true; }
  int OnDragUpdated(const ui::DropTargetEvent& event) override {
    return ui::DragDropTypes::DRAG_MOVE;
  }
  DropCallback GetDropCallback(const ui::DropTargetEvent& event) override {
    return base::BindOnce(&TargetView::PerformDrop, base::Unretained(this));
  }

  bool dropped() const { return dropped_; }

 private:
  void PerformDrop(const ui::DropTargetEvent& event,
                   ui::mojom::DragOperation& output_drag_op,
                   std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner) {
    dropped_ = true;
    output_drag_op = ui::mojom::DragOperation::kMove;
  }

  bool dropped_;
};

views::Widget* CreateWidget(std::unique_ptr<views::View> contents_view,
                            const gfx::Rect& bounds,
                            aura::Window* context) {
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.accept_events = true;
  params.bounds = bounds;
  params.context = context;
  widget->Init(std::move(params));

  widget->SetContentsView(std::move(contents_view));
  widget->Show();
  return widget;
}

void DragDropAcrossMultiDisplay_Step4(base::RepeatingClosure quit_closure) {
  ui_controls::SendMouseEventsNotifyWhenDone(ui_controls::LEFT, ui_controls::UP,
                                             quit_closure);
}

void DragDropAcrossMultiDisplay_Step3(base::RepeatingClosure quit_closure) {
  // Move to the edge of the 1st display so that the mouse
  // is moved to 2nd display by ash.
  ui_controls::SendMouseMoveNotifyWhenDone(
      399, 10, base::BindOnce(&DragDropAcrossMultiDisplay_Step4, quit_closure));
}

void DragDropAcrossMultiDisplay_Step2(base::RepeatingClosure quit_closure) {
  ui_controls::SendMouseMoveNotifyWhenDone(
      20, 10, base::BindOnce(&DragDropAcrossMultiDisplay_Step3, quit_closure));
}

void DragDropAcrossMultiDisplay_Step1(base::RepeatingClosure quit_closure) {
  ui_controls::SendMouseEventsNotifyWhenDone(
      ui_controls::LEFT, ui_controls::DOWN,
      base::BindOnce(&DragDropAcrossMultiDisplay_Step2, quit_closure));
}

}  // namespace

using DragDropTest = AshInteractiveUITestBase;

// Test if the mouse gets moved properly to another display
// during drag & drop operation.
// TODO(crbug.com/40935905): Consistently failing on Chrome OS builds.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_DragDropAcrossMultiDisplay DISABLED_DragDropAcrossMultiDisplay
#else
#define MAYBE_DragDropAcrossMultiDisplay DragDropAcrossMultiDisplay
#endif
TEST_F(DragDropTest, MAYBE_DragDropAcrossMultiDisplay) {
  UpdateDisplay("400x300,400x300");
  aura::Window::Windows root_windows = Shell::Get()->GetAllRootWindows();
  auto draggable_view = std::make_unique<DraggableTestView>();
  EXPECT_CALL(*draggable_view, GetDragOperations)
      .WillRepeatedly(testing::Return(ui::DragDropTypes::DRAG_MOVE));
  draggable_view->set_drag_controller(nullptr);
  draggable_view->SetBounds(0, 0, 100, 100);
  views::Widget* source = CreateWidget(std::move(draggable_view),
                                       gfx::Rect(0, 0, 100, 100), GetContext());

  auto target_view = std::make_unique<TargetView>();
  target_view->SetBounds(0, 0, 100, 100);
  TargetView* target_view_ptr = target_view.get();
  views::Widget* target = CreateWidget(
      std::move(target_view), gfx::Rect(400, 0, 100, 100), GetContext());

  // Make sure they're on the different root windows.
  EXPECT_EQ(root_windows[0], source->GetNativeView()->GetRootWindow());
  EXPECT_EQ(root_windows[1], target->GetNativeView()->GetRootWindow());

  base::RunLoop run_loop;
  ui_controls::SendMouseMoveNotifyWhenDone(
      10, 10,
      base::BindOnce(&DragDropAcrossMultiDisplay_Step1,
                     run_loop.QuitClosure()));

  run_loop.Run();

  EXPECT_TRUE(target_view_ptr->dropped());

  source->Close();
  target->Close();
}

}  // namespace ash
