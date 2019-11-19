// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/drag_drop/drag_drop_controller.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ui_controls_factory_ash.h"
#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/test/ui_controls.h"
#include "ui/base/test/ui_controls_aura.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

class DraggableView : public views::View {
 public:
  DraggableView() = default;
  ~DraggableView() override = default;

  // views::View overrides:
  int GetDragOperations(const gfx::Point& press_pt) override {
    return ui::DragDropTypes::DRAG_MOVE;
  }
  void WriteDragData(const gfx::Point& press_pt,
                     OSExchangeData* data) override {
    data->SetString(base::UTF8ToUTF16("test"));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DraggableView);
};

class TargetView : public views::View {
 public:
  TargetView() : dropped_(false) {}
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
  int OnPerformDrop(const ui::DropTargetEvent& event) override {
    dropped_ = true;
    return ui::DragDropTypes::DRAG_MOVE;
  }

  bool dropped() const { return dropped_; }

 private:
  bool dropped_;

  DISALLOW_COPY_AND_ASSIGN(TargetView);
};

views::Widget* CreateWidget(views::View* contents_view,
                            const gfx::Rect& bounds,
                            aura::Window* context) {
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.accept_events = true;
  params.bounds = bounds;
  params.context = context;
  widget->Init(std::move(params));

  widget->SetContentsView(contents_view);
  widget->Show();
  return widget;
}

void QuitLoop() {
  base::RunLoop::QuitCurrentWhenIdleDeprecated();
}

void DragDropAcrossMultiDisplay_Step4() {
  ui_controls::SendMouseEventsNotifyWhenDone(ui_controls::LEFT, ui_controls::UP,
                                             base::BindOnce(&QuitLoop));
}

void DragDropAcrossMultiDisplay_Step3() {
  // Move to the edge of the 1st display so that the mouse
  // is moved to 2nd display by ash.
  ui_controls::SendMouseMoveNotifyWhenDone(
      399, 10, base::BindOnce(&DragDropAcrossMultiDisplay_Step4));
}

void DragDropAcrossMultiDisplay_Step2() {
  ui_controls::SendMouseMoveNotifyWhenDone(
      20, 10, base::BindOnce(&DragDropAcrossMultiDisplay_Step3));
}

void DragDropAcrossMultiDisplay_Step1() {
  ui_controls::SendMouseEventsNotifyWhenDone(
      ui_controls::LEFT, ui_controls::DOWN,
      base::BindOnce(&DragDropAcrossMultiDisplay_Step2));
}

}  // namespace

using DragDropTest = AshTestBase;

// Test if the mouse gets moved properly to another display
// during drag & drop operation.
TEST_F(DragDropTest, DragDropAcrossMultiDisplay) {
  ui_controls::InstallUIControlsAura(test::CreateAshUIControls());

  UpdateDisplay("400x400,400x400");
  aura::Window::Windows root_windows = Shell::Get()->GetAllRootWindows();
  views::View* draggable_view = new DraggableView();
  draggable_view->set_drag_controller(NULL);
  draggable_view->SetBounds(0, 0, 100, 100);
  views::Widget* source =
      CreateWidget(draggable_view, gfx::Rect(0, 0, 100, 100), CurrentContext());

  TargetView* target_view = new TargetView();
  target_view->SetBounds(0, 0, 100, 100);
  views::Widget* target =
      CreateWidget(target_view, gfx::Rect(400, 0, 100, 100), CurrentContext());

  // Make sure they're on the different root windows.
  EXPECT_EQ(root_windows[0], source->GetNativeView()->GetRootWindow());
  EXPECT_EQ(root_windows[1], target->GetNativeView()->GetRootWindow());

  ui_controls::SendMouseMoveNotifyWhenDone(
      10, 10, base::BindOnce(&DragDropAcrossMultiDisplay_Step1));

  base::RunLoop().Run();

  EXPECT_TRUE(target_view->dropped());

  source->Close();
  target->Close();

  ui_controls::InstallUIControlsAura(nullptr);
}

}  // namespace ash
