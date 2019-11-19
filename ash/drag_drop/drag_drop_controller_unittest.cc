// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/drag_drop/drag_drop_controller.h"

#include "ash/drag_drop/drag_drop_tracker.h"
#include "ash/drag_drop/drag_image_view.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/drag_drop_client_observer.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/ui_base_switches.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/gestures/gesture_types.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/test/events_test_utils.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// A simple view that makes sure RunShellDrag is invoked on mouse drag.
class DragTestView : public views::View {
 public:
  DragTestView() : views::View() { Reset(); }

  void Reset() {
    num_drag_enters_ = 0;
    num_drag_exits_ = 0;
    num_drag_updates_ = 0;
    num_drops_ = 0;
    drag_done_received_ = false;
    long_tap_received_ = false;
  }

  int VerticalDragThreshold() {
    return views::View::GetVerticalDragThreshold();
  }

  int HorizontalDragThreshold() {
    return views::View::GetHorizontalDragThreshold();
  }

  int num_drag_enters_;
  int num_drag_exits_;
  int num_drag_updates_;
  int num_drops_;
  bool drag_done_received_;
  bool long_tap_received_;

 private:
  // View overrides:
  int GetDragOperations(const gfx::Point& press_pt) override {
    return ui::DragDropTypes::DRAG_COPY;
  }

  void WriteDragData(const gfx::Point& p, OSExchangeData* data) override {
    data->SetString(base::UTF8ToUTF16("I am being dragged"));
    gfx::ImageSkiaRep image_rep(gfx::Size(10, 20), 1.0f);
    gfx::ImageSkia image_skia(image_rep);
    data->provider().SetDragImage(image_skia, gfx::Vector2d());
  }

  bool OnMousePressed(const ui::MouseEvent& event) override { return true; }

  void OnGestureEvent(ui::GestureEvent* event) override {
    if (event->type() == ui::ET_GESTURE_LONG_TAP)
      long_tap_received_ = true;
    return;
  }

  bool GetDropFormats(
      int* formats,
      std::set<ui::ClipboardFormatType>* format_types) override {
    *formats = ui::OSExchangeData::STRING;
    return true;
  }

  bool CanDrop(const OSExchangeData& data) override { return true; }

  void OnDragEntered(const ui::DropTargetEvent& event) override {
    num_drag_enters_++;
  }

  int OnDragUpdated(const ui::DropTargetEvent& event) override {
    num_drag_updates_++;
    return ui::DragDropTypes::DRAG_COPY;
  }

  void OnDragExited() override { num_drag_exits_++; }

  int OnPerformDrop(const ui::DropTargetEvent& event) override {
    num_drops_++;
    return ui::DragDropTypes::DRAG_COPY;
  }

  void OnDragDone() override { drag_done_received_ = true; }

  DISALLOW_COPY_AND_ASSIGN(DragTestView);
};

class CompletableLinearAnimation : public gfx::LinearAnimation {
 public:
  CompletableLinearAnimation(base::TimeDelta duration,
                             int frame_rate,
                             gfx::AnimationDelegate* delegate)
      : gfx::LinearAnimation(duration, frame_rate, delegate) {}

  void Complete() { Step(start_time() + duration()); }

 private:
  DISALLOW_COPY_AND_ASSIGN(CompletableLinearAnimation);
};

class TestDragDropController : public DragDropController {
 public:
  TestDragDropController() : DragDropController() { Reset(); }

  void Reset() {
    drag_start_received_ = false;
    num_drag_updates_ = 0;
    drop_received_ = false;
    drag_canceled_ = false;
    drag_string_.clear();
  }

  int StartDragAndDrop(std::unique_ptr<ui::OSExchangeData> data,
                       aura::Window* root_window,
                       aura::Window* source_window,
                       const gfx::Point& location,
                       int operation,
                       ui::DragDropTypes::DragEventSource source) override {
    drag_start_received_ = true;
    data->GetString(&drag_string_);
    return DragDropController::StartDragAndDrop(std::move(data), root_window,
                                                source_window, location,
                                                operation, source);
  }

  void DragUpdate(aura::Window* target,
                  const ui::LocatedEvent& event) override {
    DragDropController::DragUpdate(target, event);
    num_drag_updates_++;
  }

  void Drop(aura::Window* target, const ui::LocatedEvent& event) override {
    DragDropController::Drop(target, event);
    drop_received_ = true;
  }

  void DragCancel() override {
    DragDropController::DragCancel();
    drag_canceled_ = true;
  }

  gfx::LinearAnimation* CreateCancelAnimation(
      base::TimeDelta duration,
      int frame_rate,
      gfx::AnimationDelegate* delegate) override {
    return new CompletableLinearAnimation(duration, frame_rate, delegate);
  }

  void DoDragCancel(base::TimeDelta animation_duration) override {
    DragDropController::DoDragCancel(animation_duration);
    drag_canceled_ = true;
  }

  bool drag_start_received_;
  int num_drag_updates_;
  bool drop_received_;
  bool drag_canceled_;
  base::string16 drag_string_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestDragDropController);
};

class TestObserver : public aura::client::DragDropClientObserver {
 public:
  enum class State { kNotInvoked, kDragStartedInvoked, kDragEndedInvoked };

  TestObserver() : state_(State::kNotInvoked) {}

  State state() const { return state_; }

  // aura::client::DragDropClientObserver

  void OnDragStarted() override {
    EXPECT_EQ(State::kNotInvoked, state_);
    state_ = State::kDragStartedInvoked;
  }

  void OnDragEnded() override {
    EXPECT_EQ(State::kDragStartedInvoked, state_);
    state_ = State::kDragEndedInvoked;
  }

 private:
  State state_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

class EventTargetTestDelegate : public aura::client::DragDropDelegate {
 public:
  enum class State {
    kNotInvoked,
    kDragEnteredInvoked,
    kDragUpdateInvoked,
    kPerformDropInvoked
  };

  explicit EventTargetTestDelegate(aura::Window* window) : window_(window) {}
  State state() const { return state_; }

  // aura::client::DragDropDelegate:
  void OnDragEntered(const ui::DropTargetEvent& event) override {
    EXPECT_EQ(State::kNotInvoked, state_);
    EXPECT_EQ(window_, event.target());
    state_ = State::kDragEnteredInvoked;
  }
  int OnDragUpdated(const ui::DropTargetEvent& event) override {
    EXPECT_TRUE(State::kDragEnteredInvoked == state_ ||
                State::kDragUpdateInvoked == state_);
    EXPECT_EQ(window_, event.target());
    state_ = State::kDragUpdateInvoked;
    return ui::DragDropTypes::DRAG_MOVE;
  }
  void OnDragExited() override { ADD_FAILURE(); }
  int OnPerformDrop(const ui::DropTargetEvent& event,
                    std::unique_ptr<ui::OSExchangeData> data) override {
    EXPECT_EQ(State::kDragUpdateInvoked, state_);
    EXPECT_EQ(window_, event.target());
    state_ = State::kPerformDropInvoked;
    return ui::DragDropTypes::DRAG_MOVE;
  }

 private:
  aura::Window* const window_;
  State state_{State::kNotInvoked};

  DISALLOW_COPY_AND_ASSIGN(EventTargetTestDelegate);
};

void AddViewToWidgetAndResize(views::Widget* widget, views::View* view) {
  if (!widget->GetContentsView()) {
    views::View* contents_view = new views::View;
    widget->SetContentsView(contents_view);
  }

  views::View* contents_view = widget->GetContentsView();
  contents_view->AddChildView(view);
  view->SetBounds(contents_view->width(), 0, 100, 100);
  gfx::Rect contents_view_bounds = contents_view->bounds();
  contents_view_bounds.Union(view->bounds());
  contents_view->SetBoundsRect(contents_view_bounds);
  widget->SetBounds(contents_view_bounds);
}

void DispatchGesture(ui::EventType gesture_type, gfx::Point location) {
  ui::GestureEventDetails event_details(gesture_type);
  ui::GestureEvent gesture_event(location.x(), location.y(), 0,
                                 ui::EventTimeForNow(), event_details);
  ui::EventSource* event_source =
      Shell::GetPrimaryRootWindow()->GetHost()->GetEventSource();
  ui::EventSourceTestApi event_source_test(event_source);
  ui::EventDispatchDetails details =
      event_source_test.SendEventToSink(&gesture_event);
  CHECK(!details.dispatcher_destroyed);
}

}  // namespace

class DragDropControllerTest : public AshTestBase {
 public:
  DragDropControllerTest() = default;
  ~DragDropControllerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    drag_drop_controller_.reset(new TestDragDropController);
    drag_drop_controller_->set_should_block_during_drag_drop(false);
    drag_drop_controller_->set_enabled(true);
    aura::client::SetDragDropClient(Shell::GetPrimaryRootWindow(),
                                    drag_drop_controller_.get());
  }

  void TearDown() override {
    aura::client::SetDragDropClient(Shell::GetPrimaryRootWindow(), NULL);
    drag_drop_controller_.reset();
    AshTestBase::TearDown();
  }

  void UpdateDragData() {
    drag_drop_controller_->drag_data_ = std::make_unique<ui::OSExchangeData>();
    drag_drop_controller_->drag_data_->SetString(
        base::UTF8ToUTF16("I am being dragged"));
  }

  aura::Window* GetDragWindow() { return drag_drop_controller_->drag_window_; }

  aura::Window* GetDragSourceWindow() {
    return drag_drop_controller_->drag_source_window_;
  }

  void SetDragSourceWindow(aura::Window* drag_source_window) {
    drag_drop_controller_->drag_source_window_ = drag_source_window;
    drag_source_window->AddObserver(drag_drop_controller_.get());
  }

  const gfx::ImageSkia& GetDragImage() {
    return drag_drop_controller_->drag_image_.get()->GetImage();
  }

  aura::Window* GetDragImageWindow() {
    return drag_drop_controller_->drag_image_.get()
               ? drag_drop_controller_->drag_image_->GetWidget()
                     ->GetNativeWindow()
               : NULL;
  }

  DragDropTracker* drag_drop_tracker() {
    return drag_drop_controller_->drag_drop_tracker_.get();
  }

  void CompleteCancelAnimation() {
    CompletableLinearAnimation* animation =
        static_cast<CompletableLinearAnimation*>(
            drag_drop_controller_->cancel_animation_.get());
    animation->Complete();
  }

 protected:
  std::unique_ptr<views::Widget> CreateFramelessWidget() {
    std::unique_ptr<views::Widget> widget = std::make_unique<views::Widget>();
    views::Widget::InitParams params;
    params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.context = CurrentContext();
    widget->Init(std::move(params));
    widget->Show();
    return widget;
  }

  std::unique_ptr<TestDragDropController> drag_drop_controller_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DragDropControllerTest);
};

TEST_F(DragDropControllerTest, DragDropInSingleViewTest) {
  std::unique_ptr<views::Widget> widget = CreateFramelessWidget();
  DragTestView* drag_view = new DragTestView;
  AddViewToWidgetAndResize(widget.get(), drag_view);
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     widget->GetNativeView());
  generator.PressLeftButton();

  int num_drags = 17;
  for (int i = 0; i < num_drags; ++i) {
    // Because we are not doing a blocking drag and drop, the original
    // OSDragExchangeData object is lost as soon as we return from the drag
    // initiation in DragDropController::StartDragAndDrop(). Hence we set the
    // drag_data_ to a fake drag data object that we create.
    if (i > 0)
      UpdateDragData();
    generator.MoveMouseBy(0, 1);

    // Execute any scheduled draws to process deferred mouse events.
    base::RunLoop().RunUntilIdle();
  }

  generator.ReleaseLeftButton();

  EXPECT_TRUE(drag_drop_controller_->drag_start_received_);
  EXPECT_EQ(num_drags - 1 - drag_view->VerticalDragThreshold(),
            drag_drop_controller_->num_drag_updates_);
  EXPECT_TRUE(drag_drop_controller_->drop_received_);
  EXPECT_EQ(base::UTF8ToUTF16("I am being dragged"),
            drag_drop_controller_->drag_string_);

  EXPECT_EQ(1, drag_view->num_drag_enters_);
  EXPECT_EQ(num_drags - 1 - drag_view->VerticalDragThreshold(),
            drag_view->num_drag_updates_);
  EXPECT_EQ(1, drag_view->num_drops_);
  EXPECT_EQ(0, drag_view->num_drag_exits_);
  EXPECT_TRUE(drag_view->drag_done_received_);
}

TEST_F(DragDropControllerTest, DragDropWithZeroDragUpdates) {
  std::unique_ptr<views::Widget> widget = CreateFramelessWidget();
  DragTestView* drag_view = new DragTestView;
  AddViewToWidgetAndResize(widget.get(), drag_view);
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     widget->GetNativeView());
  generator.PressLeftButton();

  int num_drags = drag_view->VerticalDragThreshold() + 1;
  for (int i = 0; i < num_drags; ++i) {
    // Because we are not doing a blocking drag and drop, the original
    // OSDragExchangeData object is lost as soon as we return from the drag
    // initiation in DragDropController::StartDragAndDrop(). Hence we set the
    // drag_data_ to a fake drag data object that we create.
    if (i > 0)
      UpdateDragData();
    generator.MoveMouseBy(0, 1);
  }

  UpdateDragData();

  generator.ReleaseLeftButton();

  EXPECT_TRUE(drag_drop_controller_->drag_start_received_);
  EXPECT_EQ(num_drags - 1 - drag_view->VerticalDragThreshold() + 1,
            drag_drop_controller_->num_drag_updates_);
  EXPECT_TRUE(drag_drop_controller_->drop_received_);

  EXPECT_EQ(1, drag_view->num_drag_enters_);
  EXPECT_EQ(num_drags - 1 - drag_view->VerticalDragThreshold() + 1,
            drag_view->num_drag_updates_);
  EXPECT_EQ(1, drag_view->num_drops_);
  EXPECT_EQ(0, drag_view->num_drag_exits_);
  EXPECT_TRUE(drag_view->drag_done_received_);
}

TEST_F(DragDropControllerTest, DragDropInMultipleViewsSingleWidgetTest) {
  std::unique_ptr<views::Widget> widget = CreateFramelessWidget();
  DragTestView* drag_view1 = new DragTestView;
  AddViewToWidgetAndResize(widget.get(), drag_view1);
  DragTestView* drag_view2 = new DragTestView;
  AddViewToWidgetAndResize(widget.get(), drag_view2);

  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  generator.MoveMouseRelativeTo(widget->GetNativeView(),
                                drag_view1->bounds().CenterPoint());
  generator.PressLeftButton();

  int num_drags = drag_view1->width();
  for (int i = 0; i < num_drags; ++i) {
    // Because we are not doing a blocking drag and drop, the original
    // OSDragExchangeData object is lost as soon as we return from the drag
    // initiation in DragDropController::StartDragAndDrop(). Hence we set the
    // drag_data_ to a fake drag data object that we create.
    if (i > 0)
      UpdateDragData();
    generator.MoveMouseBy(1, 0);

    // Execute any scheduled draws to process deferred mouse events.
    base::RunLoop().RunUntilIdle();
  }

  generator.ReleaseLeftButton();

  EXPECT_TRUE(drag_drop_controller_->drag_start_received_);
  EXPECT_EQ(num_drags - 1 - drag_view1->HorizontalDragThreshold(),
            drag_drop_controller_->num_drag_updates_);
  EXPECT_TRUE(drag_drop_controller_->drop_received_);
  EXPECT_EQ(base::UTF8ToUTF16("I am being dragged"),
            drag_drop_controller_->drag_string_);

  EXPECT_EQ(1, drag_view1->num_drag_enters_);
  int num_expected_updates =
      drag_view1->bounds().width() - drag_view1->bounds().CenterPoint().x() - 2;
  EXPECT_EQ(num_expected_updates - drag_view1->HorizontalDragThreshold(),
            drag_view1->num_drag_updates_);
  EXPECT_EQ(0, drag_view1->num_drops_);
  EXPECT_EQ(1, drag_view1->num_drag_exits_);
  EXPECT_TRUE(drag_view1->drag_done_received_);

  EXPECT_EQ(1, drag_view2->num_drag_enters_);
  num_expected_updates = num_drags - num_expected_updates - 1;
  EXPECT_EQ(num_expected_updates, drag_view2->num_drag_updates_);
  EXPECT_EQ(1, drag_view2->num_drops_);
  EXPECT_EQ(0, drag_view2->num_drag_exits_);
  EXPECT_FALSE(drag_view2->drag_done_received_);
}

TEST_F(DragDropControllerTest, DragDropInMultipleViewsMultipleWidgetsTest) {
  std::unique_ptr<views::Widget> widget1 = CreateFramelessWidget();
  DragTestView* drag_view1 = new DragTestView;
  AddViewToWidgetAndResize(widget1.get(), drag_view1);
  std::unique_ptr<views::Widget> widget2 = CreateFramelessWidget();
  DragTestView* drag_view2 = new DragTestView;
  AddViewToWidgetAndResize(widget2.get(), drag_view2);
  gfx::Rect widget1_bounds = widget1->GetClientAreaBoundsInScreen();
  gfx::Rect widget2_bounds = widget2->GetClientAreaBoundsInScreen();
  widget2->SetBounds(gfx::Rect(widget1_bounds.width(), 0,
                               widget2_bounds.width(),
                               widget2_bounds.height()));

  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     widget1->GetNativeView());
  generator.PressLeftButton();

  int num_drags = drag_view1->width();
  for (int i = 0; i < num_drags; ++i) {
    // Because we are not doing a blocking drag and drop, the original
    // OSDragExchangeData object is lost as soon as we return from the drag
    // initiation in DragDropController::StartDragAndDrop(). Hence we set the
    // drag_data_ to a fake drag data object that we create.
    if (i > 0)
      UpdateDragData();
    generator.MoveMouseBy(1, 0);

    // Execute any scheduled draws to process deferred mouse events.
    base::RunLoop().RunUntilIdle();
  }

  generator.ReleaseLeftButton();

  EXPECT_TRUE(drag_drop_controller_->drag_start_received_);
  EXPECT_EQ(num_drags - 1 - drag_view1->HorizontalDragThreshold(),
            drag_drop_controller_->num_drag_updates_);
  EXPECT_TRUE(drag_drop_controller_->drop_received_);
  EXPECT_EQ(base::UTF8ToUTF16("I am being dragged"),
            drag_drop_controller_->drag_string_);

  EXPECT_EQ(1, drag_view1->num_drag_enters_);
  int num_expected_updates =
      drag_view1->bounds().width() - drag_view1->bounds().CenterPoint().x() - 2;
  EXPECT_EQ(num_expected_updates - drag_view1->HorizontalDragThreshold(),
            drag_view1->num_drag_updates_);
  EXPECT_EQ(0, drag_view1->num_drops_);
  EXPECT_EQ(1, drag_view1->num_drag_exits_);
  EXPECT_TRUE(drag_view1->drag_done_received_);

  EXPECT_EQ(1, drag_view2->num_drag_enters_);
  num_expected_updates = num_drags - num_expected_updates - 1;
  EXPECT_EQ(num_expected_updates, drag_view2->num_drag_updates_);
  EXPECT_EQ(1, drag_view2->num_drops_);
  EXPECT_EQ(0, drag_view2->num_drag_exits_);
  EXPECT_FALSE(drag_view2->drag_done_received_);
}

TEST_F(DragDropControllerTest, ViewRemovedWhileInDragDropTest) {
  std::unique_ptr<views::Widget> widget = CreateFramelessWidget();
  std::unique_ptr<DragTestView> drag_view(new DragTestView);
  AddViewToWidgetAndResize(widget.get(), drag_view.get());

  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  generator.MoveMouseToCenterOf(widget->GetNativeView());
  generator.PressLeftButton();

  int num_drags_1 = 17;
  for (int i = 0; i < num_drags_1; ++i) {
    // Because we are not doing a blocking drag and drop, the original
    // OSDragExchangeData object is lost as soon as we return from the drag
    // initiation in DragDropController::StartDragAndDrop(). Hence we set the
    // drag_data_ to a fake drag data object that we create.
    if (i > 0)
      UpdateDragData();
    generator.MoveMouseBy(0, 1);

    // Execute any scheduled draws to process deferred mouse events.
    base::RunLoop().RunUntilIdle();
  }

  drag_view->parent()->RemoveChildView(drag_view.get());
  // View has been removed. We will not get any of the following drag updates.
  int num_drags_2 = 23;
  for (int i = 0; i < num_drags_2; ++i) {
    UpdateDragData();
    generator.MoveMouseBy(0, 1);

    // Execute any scheduled draws to process deferred mouse events.
    base::RunLoop().RunUntilIdle();
  }

  generator.ReleaseLeftButton();

  EXPECT_TRUE(drag_drop_controller_->drag_start_received_);
  EXPECT_EQ(num_drags_1 + num_drags_2 - 1 - drag_view->VerticalDragThreshold(),
            drag_drop_controller_->num_drag_updates_);
  EXPECT_TRUE(drag_drop_controller_->drop_received_);
  EXPECT_EQ(base::UTF8ToUTF16("I am being dragged"),
            drag_drop_controller_->drag_string_);

  EXPECT_EQ(1, drag_view->num_drag_enters_);
  EXPECT_EQ(num_drags_1 - 1 - drag_view->VerticalDragThreshold(),
            drag_view->num_drag_updates_);
  EXPECT_EQ(0, drag_view->num_drops_);
  EXPECT_EQ(0, drag_view->num_drag_exits_);
  EXPECT_TRUE(drag_view->drag_done_received_);
}

TEST_F(DragDropControllerTest, DragLeavesClipboardAloneTest) {
  ui::Clipboard* cb = ui::Clipboard::GetForCurrentThread();
  std::string clip_str("I am on the clipboard");
  {
    // We first copy some text to the clipboard.
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
    scw.WriteText(base::ASCIIToUTF16(clip_str));
  }
  EXPECT_TRUE(cb->IsFormatAvailable(ui::ClipboardFormatType::GetPlainTextType(),
                                    ui::ClipboardBuffer::kCopyPaste));

  std::unique_ptr<views::Widget> widget = CreateFramelessWidget();
  DragTestView* drag_view = new DragTestView;
  AddViewToWidgetAndResize(widget.get(), drag_view);

  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     widget->GetNativeView());
  generator.PressLeftButton();
  generator.MoveMouseBy(0, drag_view->VerticalDragThreshold() + 1);

  // Execute any scheduled draws to process deferred mouse events.
  base::RunLoop().RunUntilIdle();

  // Verify the clipboard contents haven't changed
  std::string result;
  EXPECT_TRUE(cb->IsFormatAvailable(ui::ClipboardFormatType::GetPlainTextType(),
                                    ui::ClipboardBuffer::kCopyPaste));
  cb->ReadAsciiText(ui::ClipboardBuffer::kCopyPaste, &result);
  EXPECT_EQ(clip_str, result);
  // Destroy the clipboard here because ash doesn't delete it.
  // crbug.com/158150.
  ui::Clipboard::DestroyClipboardForCurrentThread();
}

TEST_F(DragDropControllerTest, WindowDestroyedDuringDragDrop) {
  std::unique_ptr<views::Widget> widget = CreateFramelessWidget();
  DragTestView* drag_view = new DragTestView;
  AddViewToWidgetAndResize(widget.get(), drag_view);
  aura::Window* window = widget->GetNativeView();

  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     widget->GetNativeView());
  generator.PressLeftButton();

  int num_drags = 17;
  for (int i = 0; i < num_drags; ++i) {
    // Because we are not doing a blocking drag and drop, the original
    // OSDragExchangeData object is lost as soon as we return from the drag
    // initiation in DragDropController::StartDragAndDrop(). Hence we set the
    // drag_data_ to a fake drag data object that we create.
    if (i > 0)
      UpdateDragData();
    generator.MoveMouseBy(0, 1);

    // Execute any scheduled draws to process deferred mouse events.
    base::RunLoop().RunUntilIdle();

    if (i > drag_view->VerticalDragThreshold())
      EXPECT_EQ(window, GetDragWindow());
  }

  widget->CloseNow();
  EXPECT_FALSE(GetDragWindow());

  num_drags = 23;
  for (int i = 0; i < num_drags; ++i) {
    if (i > 0)
      UpdateDragData();
    generator.MoveMouseBy(0, 1);
    // We should not crash here.
  }

  generator.ReleaseLeftButton();

  EXPECT_TRUE(drag_drop_controller_->drag_start_received_);
  EXPECT_TRUE(drag_drop_controller_->drop_received_);
}

TEST_F(DragDropControllerTest, SyntheticEventsDuringDragDrop) {
  std::unique_ptr<views::Widget> widget = CreateFramelessWidget();
  DragTestView* drag_view = new DragTestView;
  AddViewToWidgetAndResize(widget.get(), drag_view);
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     widget->GetNativeView());
  generator.PressLeftButton();

  int num_drags = 17;
  for (int i = 0; i < num_drags; ++i) {
    // Because we are not doing a blocking drag and drop, the original
    // OSDragExchangeData object is lost as soon as we return from the drag
    // initiation in DragDropController::StartDragAndDrop(). Hence we set the
    // drag_data_ to a fake drag data object that we create.
    if (i > 0)
      UpdateDragData();
    generator.MoveMouseBy(0, 1);

    // We send a unexpected mouse move event. Note that we cannot use
    // EventGenerator since it implicitly turns these into mouse drag events.
    // The DragDropController should simply ignore these events.
    gfx::Point mouse_move_location = drag_view->bounds().CenterPoint();
    ui::MouseEvent mouse_move(ui::ET_MOUSE_MOVED, mouse_move_location,
                              mouse_move_location, ui::EventTimeForNow(), 0, 0);
    ui::EventDispatchDetails details = Shell::GetPrimaryRootWindow()
                                           ->GetHost()
                                           ->event_sink()
                                           ->OnEventFromSource(&mouse_move);
    ASSERT_FALSE(details.dispatcher_destroyed);
  }

  generator.ReleaseLeftButton();

  EXPECT_TRUE(drag_drop_controller_->drag_start_received_);
  EXPECT_EQ(num_drags - 1 - drag_view->VerticalDragThreshold(),
            drag_drop_controller_->num_drag_updates_);
  EXPECT_TRUE(drag_drop_controller_->drop_received_);
  EXPECT_EQ(base::UTF8ToUTF16("I am being dragged"),
            drag_drop_controller_->drag_string_);

  EXPECT_EQ(1, drag_view->num_drag_enters_);
  EXPECT_EQ(num_drags - 1 - drag_view->VerticalDragThreshold(),
            drag_view->num_drag_updates_);
  EXPECT_EQ(1, drag_view->num_drops_);
  EXPECT_EQ(0, drag_view->num_drag_exits_);
  EXPECT_TRUE(drag_view->drag_done_received_);
}

TEST_F(DragDropControllerTest, PressingEscapeCancelsDragDrop) {
  std::unique_ptr<views::Widget> widget = CreateFramelessWidget();
  DragTestView* drag_view = new DragTestView;
  AddViewToWidgetAndResize(widget.get(), drag_view);
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     widget->GetNativeView());
  generator.PressLeftButton();

  int num_drags = 17;
  for (int i = 0; i < num_drags; ++i) {
    // Because we are not doing a blocking drag and drop, the original
    // OSDragExchangeData object is lost as soon as we return from the drag
    // initiation in DragDropController::StartDragAndDrop(). Hence we set the
    // drag_data_ to a fake drag data object that we created.
    if (i > 0)
      UpdateDragData();
    generator.MoveMouseBy(0, 1);

    // Execute any scheduled draws to process deferred mouse events.
    base::RunLoop().RunUntilIdle();
  }

  generator.PressKey(ui::VKEY_ESCAPE, 0);

  EXPECT_TRUE(drag_drop_controller_->drag_start_received_);
  EXPECT_EQ(num_drags - 1 - drag_view->VerticalDragThreshold(),
            drag_drop_controller_->num_drag_updates_);
  EXPECT_FALSE(drag_drop_controller_->drop_received_);
  EXPECT_TRUE(drag_drop_controller_->drag_canceled_);
  EXPECT_EQ(base::UTF8ToUTF16("I am being dragged"),
            drag_drop_controller_->drag_string_);

  EXPECT_EQ(1, drag_view->num_drag_enters_);
  EXPECT_EQ(num_drags - 1 - drag_view->VerticalDragThreshold(),
            drag_view->num_drag_updates_);
  EXPECT_EQ(0, drag_view->num_drops_);
  EXPECT_EQ(1, drag_view->num_drag_exits_);
  EXPECT_TRUE(drag_view->drag_done_received_);
}

TEST_F(DragDropControllerTest, CaptureLostCancelsDragDrop) {
  std::unique_ptr<views::Widget> widget = CreateFramelessWidget();
  DragTestView* drag_view = new DragTestView;
  AddViewToWidgetAndResize(widget.get(), drag_view);
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     widget->GetNativeView());
  generator.PressLeftButton();

  int num_drags = 17;
  for (int i = 0; i < num_drags; ++i) {
    // Because we are not doing a blocking drag and drop, the original
    // OSDragExchangeData object is lost as soon as we return from the drag
    // initiation in DragDropController::StartDragAndDrop(). Hence we set the
    // drag_data_ to a fake drag data object that we create.
    if (i > 0)
      UpdateDragData();
    generator.MoveMouseBy(0, 1);

    // Execute any scheduled draws to process deferred mouse events.
    base::RunLoop().RunUntilIdle();
  }
  // Make sure the capture window won't handle mouse events.
  aura::Window* capture_window = drag_drop_tracker()->capture_window();
  ASSERT_TRUE(capture_window);
  EXPECT_EQ("0x0", capture_window->bounds().size().ToString());
  EXPECT_EQ(NULL, capture_window->GetEventHandlerForPoint(gfx::Point()));

  aura::client::GetCaptureClient(widget->GetNativeView()->GetRootWindow())
      ->SetCapture(NULL);

  EXPECT_TRUE(drag_drop_controller_->drag_start_received_);
  EXPECT_EQ(num_drags - 1 - drag_view->VerticalDragThreshold(),
            drag_drop_controller_->num_drag_updates_);
  EXPECT_FALSE(drag_drop_controller_->drop_received_);
  EXPECT_TRUE(drag_drop_controller_->drag_canceled_);
  EXPECT_EQ(base::UTF8ToUTF16("I am being dragged"),
            drag_drop_controller_->drag_string_);

  EXPECT_EQ(1, drag_view->num_drag_enters_);
  EXPECT_EQ(num_drags - 1 - drag_view->VerticalDragThreshold(),
            drag_view->num_drag_updates_);
  EXPECT_EQ(0, drag_view->num_drops_);
  EXPECT_EQ(1, drag_view->num_drag_exits_);
  EXPECT_TRUE(drag_view->drag_done_received_);
}

TEST_F(DragDropControllerTest, TouchDragDropInMultipleWindows) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableTouchDragDrop);
  std::unique_ptr<views::Widget> widget1 = CreateFramelessWidget();
  DragTestView* drag_view1 = new DragTestView;
  AddViewToWidgetAndResize(widget1.get(), drag_view1);
  std::unique_ptr<views::Widget> widget2 = CreateFramelessWidget();
  DragTestView* drag_view2 = new DragTestView;
  AddViewToWidgetAndResize(widget2.get(), drag_view2);
  gfx::Rect widget1_bounds = widget1->GetClientAreaBoundsInScreen();
  gfx::Rect widget2_bounds = widget2->GetClientAreaBoundsInScreen();
  widget2->SetBounds(gfx::Rect(widget1_bounds.width(), 0,
                               widget2_bounds.width(),
                               widget2_bounds.height()));

  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     widget1->GetNativeView());
  generator.PressTouch();
  gfx::Point point = gfx::Rect(drag_view1->bounds()).CenterPoint();
  DispatchGesture(ui::ET_GESTURE_LONG_PRESS, point);
  // Because we are not doing a blocking drag and drop, the original
  // OSDragExchangeData object is lost as soon as we return from the drag
  // initiation in DragDropController::StartDragAndDrop(). Hence we set the
  // drag_data_ to a fake drag data object that we create.
  UpdateDragData();
  gfx::Point gesture_location = point;
  int num_drags = drag_view1->width();
  for (int i = 0; i < num_drags; ++i) {
    gesture_location.Offset(1, 0);
    DispatchGesture(ui::ET_GESTURE_SCROLL_UPDATE, gesture_location);

    // Execute any scheduled draws to process deferred mouse events.
    base::RunLoop().RunUntilIdle();
  }

  DispatchGesture(ui::ET_GESTURE_SCROLL_END, gesture_location);

  EXPECT_TRUE(drag_drop_controller_->drag_start_received_);
  EXPECT_EQ(num_drags, drag_drop_controller_->num_drag_updates_);
  EXPECT_TRUE(drag_drop_controller_->drop_received_);
  EXPECT_EQ(base::UTF8ToUTF16("I am being dragged"),
            drag_drop_controller_->drag_string_);

  EXPECT_EQ(1, drag_view1->num_drag_enters_);
  int num_expected_updates =
      drag_view1->bounds().width() - drag_view1->bounds().CenterPoint().x() - 1;
  EXPECT_EQ(num_expected_updates, drag_view1->num_drag_updates_);
  EXPECT_EQ(0, drag_view1->num_drops_);
  EXPECT_EQ(1, drag_view1->num_drag_exits_);
  EXPECT_TRUE(drag_view1->drag_done_received_);

  EXPECT_EQ(1, drag_view2->num_drag_enters_);
  num_expected_updates = num_drags - num_expected_updates;
  EXPECT_EQ(num_expected_updates, drag_view2->num_drag_updates_);
  EXPECT_EQ(1, drag_view2->num_drops_);
  EXPECT_EQ(0, drag_view2->num_drag_exits_);
  EXPECT_FALSE(drag_view2->drag_done_received_);
}

TEST_F(DragDropControllerTest, TouchDragDropCancelsOnLongTap) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableTouchDragDrop);
  std::unique_ptr<views::Widget> widget = CreateFramelessWidget();
  DragTestView* drag_view = new DragTestView;
  AddViewToWidgetAndResize(widget.get(), drag_view);
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     widget->GetNativeView());

  generator.PressTouch();
  gfx::Point point = gfx::Rect(drag_view->bounds()).CenterPoint();
  DispatchGesture(ui::ET_GESTURE_LONG_PRESS, point);
  DispatchGesture(ui::ET_GESTURE_LONG_TAP, point);

  EXPECT_TRUE(drag_drop_controller_->drag_start_received_);
  EXPECT_TRUE(drag_drop_controller_->drag_canceled_);
  EXPECT_EQ(0, drag_drop_controller_->num_drag_updates_);
  EXPECT_FALSE(drag_drop_controller_->drop_received_);
  EXPECT_EQ(base::UTF8ToUTF16("I am being dragged"),
            drag_drop_controller_->drag_string_);
  EXPECT_EQ(0, drag_view->num_drag_enters_);
  EXPECT_EQ(0, drag_view->num_drops_);
  EXPECT_EQ(0, drag_view->num_drag_exits_);
  EXPECT_TRUE(drag_view->drag_done_received_);
}

TEST_F(DragDropControllerTest, TouchDragDropLongTapGestureIsForwarded) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableTouchDragDrop);
  std::unique_ptr<views::Widget> widget = CreateFramelessWidget();
  DragTestView* drag_view = new DragTestView;
  AddViewToWidgetAndResize(widget.get(), drag_view);
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     widget->GetNativeView());

  generator.PressTouch();
  gfx::Point point = gfx::Rect(drag_view->bounds()).CenterPoint();
  DispatchGesture(ui::ET_GESTURE_LONG_PRESS, point);

  // Since we are not running inside a nested loop, the |drag_source_window_|
  // will get destroyed immediately. Hence we reassign it.
  EXPECT_EQ(NULL, GetDragSourceWindow());
  SetDragSourceWindow(widget->GetNativeView());
  EXPECT_FALSE(drag_view->long_tap_received_);
  DispatchGesture(ui::ET_GESTURE_LONG_TAP, point);
  CompleteCancelAnimation();
  EXPECT_TRUE(drag_view->long_tap_received_);
}

TEST_F(DragDropControllerTest, DragDropWithChangingIcon) {
  std::unique_ptr<views::Widget> widget = CreateFramelessWidget();
  DragTestView* drag_view1 = new DragTestView;
  AddViewToWidgetAndResize(widget.get(), drag_view1);
  DragTestView* drag_view2 = new DragTestView;
  AddViewToWidgetAndResize(widget.get(), drag_view2);

  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  generator.MoveMouseRelativeTo(widget->GetNativeView(),
                                drag_view1->bounds().CenterPoint());
  generator.PressLeftButton();

  int num_drags = drag_view1->width();
  int icon_replacements = 0;
  for (int i = 0; i < num_drags; ++i) {
    // Because we are not doing a blocking drag and drop, the original
    // OSDragExchangeData object is lost as soon as we return from the drag
    // initiation in DragDropController::StartDragAndDrop(). Hence we set the
    // drag_data_ to a fake drag data object that we create.
    if (i > 0) {
      UpdateDragData();
    }

    if (drag_drop_controller_->IsDragDropInProgress()) {
      if (!GetDragSourceWindow())
        SetDragSourceWindow(widget->GetNativeWindow());

      gfx::ImageSkia new_icon;
      new_icon.AddRepresentation(gfx::ImageSkiaRep(gfx::Size(10, 10), 1.0f));

      EXPECT_FALSE(GetDragImage().BackedBySameObjectAs(new_icon));
      drag_drop_controller_->SetDragImage(new_icon, gfx::Vector2d());
      EXPECT_TRUE(GetDragImage().BackedBySameObjectAs(new_icon));
      icon_replacements++;
    }

    generator.MoveMouseBy(1, 0);

    // Execute any scheduled draws to process deferred mouse events.
    base::RunLoop().RunUntilIdle();
  }

  generator.ReleaseLeftButton();

  EXPECT_GT(icon_replacements, 0);

  EXPECT_TRUE(drag_drop_controller_->drag_start_received_);
  EXPECT_EQ(num_drags - 1 - drag_view1->HorizontalDragThreshold(),
            drag_drop_controller_->num_drag_updates_);
  EXPECT_TRUE(drag_drop_controller_->drop_received_);
  EXPECT_EQ(base::UTF8ToUTF16("I am being dragged"),
            drag_drop_controller_->drag_string_);

  EXPECT_EQ(1, drag_view1->num_drag_enters_);
  int num_expected_updates =
      drag_view1->bounds().width() - drag_view1->bounds().CenterPoint().x() - 2;
  EXPECT_EQ(num_expected_updates - drag_view1->HorizontalDragThreshold(),
            drag_view1->num_drag_updates_);
  EXPECT_EQ(0, drag_view1->num_drops_);
  EXPECT_EQ(1, drag_view1->num_drag_exits_);
  EXPECT_TRUE(drag_view1->drag_done_received_);

  EXPECT_EQ(1, drag_view2->num_drag_enters_);
  num_expected_updates = num_drags - num_expected_updates - 1;
  EXPECT_EQ(num_expected_updates, drag_view2->num_drag_updates_);
  EXPECT_EQ(1, drag_view2->num_drops_);
  EXPECT_EQ(0, drag_view2->num_drag_exits_);
  EXPECT_FALSE(drag_view2->drag_done_received_);
}

namespace {

class DragImageWindowObserver : public aura::WindowObserver {
 public:
  void OnWindowDestroying(aura::Window* window) override {
    window_location_on_destroying_ = window->GetBoundsInScreen().origin();
  }

  gfx::Point window_location_on_destroying() const {
    return window_location_on_destroying_;
  }

 public:
  gfx::Point window_location_on_destroying_;
};

}  // namespace

// Verifies the drag image moves back to the position where drag is started
// across displays when drag is cancelled.
TEST_F(DragDropControllerTest, DragCancelAcrossDisplays) {
  UpdateDisplay("400x400,400x400");
  aura::Window::Windows root_windows = Shell::Get()->GetAllRootWindows();
  for (aura::Window::Windows::iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter) {
    aura::client::SetDragDropClient(*iter, drag_drop_controller_.get());
  }

  {
    auto data(std::make_unique<ui::OSExchangeData>());
    data->SetString(base::UTF8ToUTF16("I am being dragged"));
    std::unique_ptr<views::Widget> widget = CreateFramelessWidget();
    aura::Window* window = widget->GetNativeWindow();
    drag_drop_controller_->StartDragAndDrop(
        std::move(data), window->GetRootWindow(), window, gfx::Point(5, 5),
        ui::DragDropTypes::DRAG_MOVE,
        ui::DragDropTypes::DRAG_EVENT_SOURCE_MOUSE);

    DragImageWindowObserver observer;
    ASSERT_TRUE(GetDragImageWindow());
    GetDragImageWindow()->AddObserver(&observer);

    {
      ui::MouseEvent e(ui::ET_MOUSE_DRAGGED, gfx::Point(200, 0),
                       gfx::Point(200, 0), ui::EventTimeForNow(), ui::EF_NONE,
                       ui::EF_NONE);
      drag_drop_controller_->DragUpdate(window, e);
    }
    {
      ui::MouseEvent e(ui::ET_MOUSE_DRAGGED, gfx::Point(600, 0),
                       gfx::Point(600, 0), ui::EventTimeForNow(), ui::EF_NONE,
                       ui::EF_NONE);
      drag_drop_controller_->DragUpdate(window, e);
    }

    drag_drop_controller_->DragCancel();
    CompleteCancelAnimation();

    EXPECT_EQ("5,5", observer.window_location_on_destroying().ToString());
  }

  {
    auto data(std::make_unique<ui::OSExchangeData>());
    data->SetString(base::UTF8ToUTF16("I am being dragged"));
    std::unique_ptr<views::Widget> widget = CreateFramelessWidget();
    aura::Window* window = widget->GetNativeWindow();
    drag_drop_controller_->StartDragAndDrop(
        std::move(data), window->GetRootWindow(), window, gfx::Point(405, 405),
        ui::DragDropTypes::DRAG_MOVE,
        ui::DragDropTypes::DRAG_EVENT_SOURCE_MOUSE);
    DragImageWindowObserver observer;
    ASSERT_TRUE(GetDragImageWindow());
    GetDragImageWindow()->AddObserver(&observer);

    {
      ui::MouseEvent e(ui::ET_MOUSE_DRAGGED, gfx::Point(600, 0),
                       gfx::Point(600, 0), ui::EventTimeForNow(), ui::EF_NONE,
                       ui::EF_NONE);
      drag_drop_controller_->DragUpdate(window, e);
    }
    {
      ui::MouseEvent e(ui::ET_MOUSE_DRAGGED, gfx::Point(200, 0),
                       gfx::Point(200, 0), ui::EventTimeForNow(), ui::EF_NONE,
                       ui::EF_NONE);
      drag_drop_controller_->DragUpdate(window, e);
    }

    drag_drop_controller_->DragCancel();
    CompleteCancelAnimation();

    EXPECT_EQ("405,405", observer.window_location_on_destroying().ToString());
  }
  for (aura::Window::Windows::iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter) {
    aura::client::SetDragDropClient(*iter, NULL);
  }
}

// Verifies that a drag is aborted if a display is disconnected during the drag.
TEST_F(DragDropControllerTest, DragCancelOnDisplayDisconnect) {
  UpdateDisplay("400x400,400x400");
  for (aura::Window* root : Shell::Get()->GetAllRootWindows()) {
    aura::client::SetDragDropClient(root, drag_drop_controller_.get());
  }

  auto data(std::make_unique<ui::OSExchangeData>());
  data->SetString(base::UTF8ToUTF16("I am being dragged"));
  std::unique_ptr<views::Widget> widget = CreateFramelessWidget();
  aura::Window* window = widget->GetNativeWindow();
  drag_drop_controller_->StartDragAndDrop(
      std::move(data), window->GetRootWindow(), window, gfx::Point(5, 5),
      ui::DragDropTypes::DRAG_MOVE, ui::DragDropTypes::DRAG_EVENT_SOURCE_MOUSE);

  // Start dragging.
  ui::MouseEvent e1(ui::ET_MOUSE_DRAGGED, gfx::Point(200, 0),
                    gfx::Point(200, 0), ui::EventTimeForNow(), ui::EF_NONE,
                    ui::EF_NONE);
  drag_drop_controller_->DragUpdate(window, e1);
  EXPECT_TRUE(drag_drop_controller_->drag_start_received_);
  EXPECT_TRUE(drag_drop_controller_->IsDragDropInProgress());

  // Drag onto the secondary display.
  ui::MouseEvent e2(ui::ET_MOUSE_DRAGGED, gfx::Point(600, 0),
                    gfx::Point(600, 0), ui::EventTimeForNow(), ui::EF_NONE,
                    ui::EF_NONE);
  drag_drop_controller_->DragUpdate(window, e2);
  EXPECT_TRUE(drag_drop_controller_->IsDragDropInProgress());

  // Disconnect the secondary display.
  UpdateDisplay("800x600");

  // The drag is canceled.
  EXPECT_TRUE(drag_drop_controller_->drag_canceled_);
  EXPECT_FALSE(drag_drop_controller_->IsDragDropInProgress());
}

TEST_F(DragDropControllerTest, TouchDragDropCompletesOnFling) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableTouchDragDrop);
  ui::GestureConfiguration::GetInstance()
      ->set_max_touch_move_in_pixels_for_click(1);
  std::unique_ptr<views::Widget> widget = CreateFramelessWidget();
  DragTestView* drag_view = new DragTestView;
  AddViewToWidgetAndResize(widget.get(), drag_view);
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     widget->GetNativeView());

  gfx::Point start = gfx::Rect(drag_view->bounds()).CenterPoint();
  gfx::Point mid = start + gfx::Vector2d(drag_view->bounds().width() / 6, 0);
  gfx::Point end = start + gfx::Vector2d(drag_view->bounds().width() / 3, 0);

  base::TimeTicks timestamp = ui::EventTimeForNow();
  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, start, timestamp,
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  generator.Dispatch(&press);

  DispatchGesture(ui::ET_GESTURE_LONG_PRESS, start);
  UpdateDragData();
  timestamp += base::TimeDelta::FromMilliseconds(10);
  ui::TouchEvent move1(
      ui::ET_TOUCH_MOVED, mid, timestamp,
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  generator.Dispatch(&move1);
  // Doing two moves instead of one will guarantee to generate a fling at the
  // end.
  timestamp += base::TimeDelta::FromMilliseconds(10);
  ui::TouchEvent move2(
      ui::ET_TOUCH_MOVED, end, timestamp,
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  generator.Dispatch(&move2);
  ui::TouchEvent release(
      ui::ET_TOUCH_RELEASED, end, timestamp,
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  generator.Dispatch(&release);

  EXPECT_TRUE(drag_drop_controller_->drag_start_received_);
  EXPECT_FALSE(drag_drop_controller_->drag_canceled_);
  EXPECT_EQ(2, drag_drop_controller_->num_drag_updates_);
  EXPECT_TRUE(drag_drop_controller_->drop_received_);
  EXPECT_EQ(base::UTF8ToUTF16("I am being dragged"),
            drag_drop_controller_->drag_string_);
  EXPECT_EQ(1, drag_view->num_drag_enters_);
  EXPECT_EQ(2, drag_view->num_drag_updates_);
  EXPECT_EQ(1, drag_view->num_drops_);
  EXPECT_EQ(0, drag_view->num_drag_exits_);
  EXPECT_TRUE(drag_view->drag_done_received_);
}

TEST_F(DragDropControllerTest, DragStartedAndEndedEvents) {
  TestObserver observer;
  drag_drop_controller_->AddObserver(&observer);

  {
    auto data(std::make_unique<ui::OSExchangeData>());
    data->SetString(base::UTF8ToUTF16("I am being dragged"));
    std::unique_ptr<views::Widget> widget = CreateFramelessWidget();
    aura::Window* window = widget->GetNativeWindow();
    drag_drop_controller_->StartDragAndDrop(
        std::move(data), window->GetRootWindow(), window, gfx::Point(5, 5),
        ui::DragDropTypes::DRAG_MOVE,
        ui::DragDropTypes::DRAG_EVENT_SOURCE_MOUSE);

    EXPECT_EQ(TestObserver::State::kDragStartedInvoked, observer.state());

    ui::MouseEvent e(ui::ET_MOUSE_DRAGGED, gfx::Point(200, 0),
                     gfx::Point(200, 0), ui::EventTimeForNow(), ui::EF_NONE,
                     ui::EF_NONE);
    drag_drop_controller_->Drop(window, e);

    EXPECT_EQ(TestObserver::State::kDragEndedInvoked, observer.state());
  }

  drag_drop_controller_->RemoveObserver(&observer);
}

TEST_F(DragDropControllerTest, SetEnabled) {
  TestObserver observer;
  drag_drop_controller_->AddObserver(&observer);

  // Data for the drag.
  auto data(std::make_unique<ui::OSExchangeData>());
  data->SetString(base::UTF8ToUTF16("I am being dragged"));
  std::unique_ptr<views::Widget> widget = CreateFramelessWidget();
  aura::Window* window = widget->GetNativeWindow();

  // Cannot start a drag when the controller is disabled.
  drag_drop_controller_->set_enabled(false);
  drag_drop_controller_->StartDragAndDrop(
      std::move(data), window->GetRootWindow(), window, gfx::Point(5, 5),
      ui::DragDropTypes::DRAG_MOVE, ui::DragDropTypes::DRAG_EVENT_SOURCE_MOUSE);
  EXPECT_EQ(TestObserver::State::kNotInvoked, observer.state());

  drag_drop_controller_->RemoveObserver(&observer);
}

TEST_F(DragDropControllerTest, EventTarget) {
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate(), -1,
      gfx::Rect(0, 0, 100, 100)));
  EventTargetTestDelegate delegate(window.get());
  aura::client::SetDragDropDelegate(window.get(), &delegate);

  // Posted task will be run when the inner loop runs in StartDragAndDrop.
  ui::test::EventGenerator generator(window->GetRootWindow(), window.get());
  generator.PressLeftButton();
  // For drag enter
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&ui::test::EventGenerator::MoveMouseBy,
                                base::Unretained(&generator), 0, 1));
  // For drag update
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&ui::test::EventGenerator::MoveMouseBy,
                                base::Unretained(&generator), 0, 1));
  // For perform drop
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&ui::test::EventGenerator::ReleaseLeftButton,
                                base::Unretained(&generator)));

  drag_drop_controller_->set_should_block_during_drag_drop(true);
  auto data(std::make_unique<ui::OSExchangeData>());
  data->SetString(base::UTF8ToUTF16("I am being dragged"));
  drag_drop_controller_->StartDragAndDrop(
      std::move(data), window->GetRootWindow(), window.get(), gfx::Point(5, 5),
      ui::DragDropTypes::DRAG_MOVE, ui::DragDropTypes::DRAG_EVENT_SOURCE_MOUSE);

  EXPECT_EQ(EventTargetTestDelegate::State::kPerformDropInvoked,
            delegate.state());
  base::RunLoop().RunUntilIdle();
}

}  // namespace ash
