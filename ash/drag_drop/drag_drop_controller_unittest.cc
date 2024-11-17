// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/drag_drop/drag_drop_controller.h"

#include <memory>
#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/drag_drop/drag_image_view.h"
#include "ash/drag_drop/mock_drag_drop_observer.h"
#include "ash/drag_drop/toplevel_window_drag_delegate.h"
#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test_shell_delegate.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_types.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/drag_drop_client_observer.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/data_transfer_policy/data_transfer_policy_controller.h"
#include "ui/base/data_transfer_policy/mock_data_transfer_policy_controller.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/gestures/gesture_types.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/test/events_test_utils.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::ui::mojom::DragOperation;

// Sets string and drag image for testing.
void SetDragData(OSExchangeData* data, bool with_image) {
  data->SetString(u"I am being dragged");
  if (with_image) {
    gfx::ImageSkiaRep image_rep(gfx::Size(10, 20), 1.0f);
    gfx::ImageSkia image_skia(image_rep);
    data->provider().SetDragImage(image_skia, gfx::Vector2d());
  }
}

std::unique_ptr<ui::OSExchangeData> CreateDragData(bool with_image) {
  auto data = std::make_unique<ui::OSExchangeData>();
  SetDragData(data.get(), with_image);
  return data;
}

// A simple view that makes sure RunShellDrag is invoked on mouse drag.
class DragTestView : public views::View {
 public:
  DragTestView() : views::View() { Reset(); }

  DragTestView(const DragTestView&) = delete;
  DragTestView& operator=(const DragTestView&) = delete;

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

  void OmitDragImage() { omit_drag_image_ = true; }

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
    SetDragData(data, /*with_image=*/!omit_drag_image_);
  }

  bool OnMousePressed(const ui::MouseEvent& event) override { return true; }

  void OnGestureEvent(ui::GestureEvent* event) override {
    if (event->type() == ui::EventType::kGestureLongTap) {
      long_tap_received_ = true;
    }
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

  DropCallback GetDropCallback(const ui::DropTargetEvent& event) override {
    return base::BindOnce(&DragTestView::PerformDrop, base::Unretained(this));
  }

  void OnDragDone() override { drag_done_received_ = true; }

  void PerformDrop(const ui::DropTargetEvent& event,
                   ui::mojom::DragOperation& output_drag_op,
                   std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner) {
    num_drops_++;
    output_drag_op = DragOperation::kCopy;
  }

  bool omit_drag_image_ = false;
};

class CompletableLinearAnimation : public gfx::LinearAnimation {
 public:
  CompletableLinearAnimation(base::TimeDelta duration,
                             int frame_rate,
                             gfx::AnimationDelegate* delegate)
      : gfx::LinearAnimation(duration, frame_rate, delegate) {}

  CompletableLinearAnimation(const CompletableLinearAnimation&) = delete;
  CompletableLinearAnimation& operator=(const CompletableLinearAnimation&) =
      delete;

  void Complete() { Step(start_time() + duration()); }
};

class TestDragDropController : public DragDropController {
 public:
  TestDragDropController() : DragDropController() { Reset(); }

  TestDragDropController(const TestDragDropController&) = delete;
  TestDragDropController& operator=(const TestDragDropController&) = delete;

  void Reset() {
    drag_start_received_ = false;
    num_drag_updates_ = 0;
    drop_received_ = false;
    drag_canceled_ = false;
    drag_string_.reset();
  }

  DragOperation StartDragAndDrop(std::unique_ptr<ui::OSExchangeData> data,
                                 aura::Window* root_window,
                                 aura::Window* source_window,
                                 const gfx::Point& location,
                                 int allowed_operations,
                                 ui::mojom::DragEventSource source) override {
    drag_start_received_ = true;
    drag_string_ = data->GetString();
    return DragDropController::StartDragAndDrop(std::move(data), root_window,
                                                source_window, location,
                                                allowed_operations, source);
  }

  DragDropCaptureDelegate* get_capture_delegate() {
    return DragDropController::get_capture_delegate();
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
  std::optional<std::u16string> drag_string_;
};

class TestObserver : public aura::client::DragDropClientObserver {
 public:
  enum class State { kNotInvoked, kDragStartedInvoked, kDragEndedInvoked };

  TestObserver() : state_(State::kNotInvoked) {}

  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;

  State state() const { return state_; }

  // aura::client::DragDropClientObserver
  void OnDragStarted() override {
    EXPECT_EQ(State::kNotInvoked, state_);
    state_ = State::kDragStartedInvoked;
  }

  void OnDragCompleted(const ui::DropTargetEvent& event) override {
    EXPECT_EQ(State::kDragStartedInvoked, state_);
    state_ = State::kDragEndedInvoked;
  }

 private:
  State state_;
};

class EventTargetTestDelegate : public aura::client::DragDropDelegate {
 public:
  enum class State {
    kNotInvoked,
    kDragEnteredInvoked,
    kDragUpdateInvoked,
    kPerformDropInvoked,
    kDragExitInvoked
  };

  explicit EventTargetTestDelegate(aura::Window* window) : window_(window) {}

  EventTargetTestDelegate(const EventTargetTestDelegate&) = delete;
  EventTargetTestDelegate& operator=(const EventTargetTestDelegate&) = delete;

  State state() const { return state_; }

  // aura::client::DragDropDelegate:
  void OnDragEntered(const ui::DropTargetEvent& event) override {
    EXPECT_EQ(State::kNotInvoked, state_);
    EXPECT_EQ(window_, event.target());
    state_ = State::kDragEnteredInvoked;
  }
  aura::client::DragUpdateInfo OnDragUpdated(
      const ui::DropTargetEvent& event) override {
    EXPECT_TRUE(State::kDragEnteredInvoked == state_ ||
                State::kDragUpdateInvoked == state_);
    EXPECT_EQ(window_, event.target());
    state_ = State::kDragUpdateInvoked;
    return aura::client::DragUpdateInfo(
        ui::DragDropTypes::DRAG_MOVE,
        ui::DataTransferEndpoint(ui::EndpointType::kDefault));
  }
  void OnDragExited() override {
    EXPECT_TRUE(State::kDragEnteredInvoked == state_ ||
                State::kDragUpdateInvoked == state_);
    state_ = State::kDragExitInvoked;
  }
  DropCallback GetDropCallback(const ui::DropTargetEvent& event) override {
    return base::BindOnce(&EventTargetTestDelegate::PerformDrop,
                          base::Unretained(this));
  }

 private:
  void PerformDrop(std::unique_ptr<ui::OSExchangeData> data,
                   ui::mojom::DragOperation& output_drag_op,
                   std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner) {
    EXPECT_EQ(State::kDragUpdateInvoked, state_);

    state_ = State::kPerformDropInvoked;
    output_drag_op = DragOperation::kMove;
  }

  const raw_ptr<aura::Window, DanglingUntriaged> window_;
  State state_{State::kNotInvoked};
};

void AddViewToWidgetAndResize(views::Widget* widget, views::View* view) {
  if (!widget->GetContentsView())
    widget->SetContentsView(std::make_unique<views::View>());

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

class TestToplevelWindowDragDelegate : public ToplevelWindowDragDelegate {
 public:
  enum class State {
    kNotInvoked,
    kDragStartedInvoked,
    kDragDroppedInvoked,
    kDragCancelledInvoked,
    kDragEventInvoked
  };

  TestToplevelWindowDragDelegate() = default;

  TestToplevelWindowDragDelegate(const TestToplevelWindowDragDelegate&) =
      delete;
  TestToplevelWindowDragDelegate& operator=(
      const TestToplevelWindowDragDelegate&) = delete;

  ~TestToplevelWindowDragDelegate() override = default;

  State state() const { return state_; }
  int events_forwarded() const { return events_forwarded_; }
  ui::mojom::DragEventSource source() const { return source_; }
  std::optional<gfx::PointF> current_location() const {
    return current_location_;
  }

  // ToplevelWindowDragDelegate:
  void OnToplevelWindowDragStarted(const gfx::PointF& start_location,
                                   ui::mojom::DragEventSource source,
                                   aura::Window* source_window) override {
    ASSERT_EQ(State::kNotInvoked, state_);
    ASSERT_TRUE(source_window);
    state_ = State::kDragStartedInvoked;
    current_location_.emplace(start_location);
    source_ = source;
    if (source == ui::mojom::DragEventSource::kMouse)
      source_window->SetCapture();
  }

  DragOperation OnToplevelWindowDragDropped() override {
    EXPECT_EQ(State::kDragStartedInvoked, state_);
    state_ = State::kDragDroppedInvoked;
    return DragOperation::kMove;
  }

  void OnToplevelWindowDragCancelled() override {
    EXPECT_EQ(State::kDragStartedInvoked, state_);
    state_ = State::kDragCancelledInvoked;
  }

  void OnToplevelWindowDragEvent(ui::LocatedEvent* event) override {
    ASSERT_TRUE(event);
    EXPECT_TRUE(current_location_.has_value());
    current_location_.emplace(event->root_location_f());
    events_forwarded_++;
    event->StopPropagation();
  }

 private:
  State state_ = State::kNotInvoked;
  int events_forwarded_ = 0;
  std::optional<gfx::PointF> current_location_;
  ui::mojom::DragEventSource source_;
};

}  // namespace

class MockShellDelegate : public TestShellDelegate {
 public:
  MockShellDelegate() = default;
  ~MockShellDelegate() override = default;

  MOCK_METHOD(bool, IsTabDrag, (const ui::OSExchangeData&), (override));
};

class MockNewWindowDelegate : public TestNewWindowDelegate {
 public:
  MockNewWindowDelegate() = default;
  ~MockNewWindowDelegate() override = default;

  MOCK_METHOD(void,
              NewWindowForDetachingTab,
              (aura::Window*,
               const ui::OSExchangeData&,
               NewWindowForDetachingTabCallback),
              (override));
};

class DragDropControllerTest : public AshTestBase {
 public:
  DragDropControllerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  DragDropControllerTest(const DragDropControllerTest&) = delete;
  DragDropControllerTest& operator=(const DragDropControllerTest&) = delete;

  ~DragDropControllerTest() override = default;

  void SetUp() override {
    auto mock_shell_delegate = std::make_unique<NiceMock<MockShellDelegate>>();
    mock_shell_delegate_ = mock_shell_delegate.get();
    AshTestBase::SetUp(std::move(mock_shell_delegate));

    drag_drop_controller_ = std::make_unique<TestDragDropController>();
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

  aura::Window* GetDragWindow() { return drag_drop_controller_->drag_window_; }

  aura::Window* GetDragSourceWindow() {
    return drag_drop_controller_->drag_source_window_;
  }

  void SetDragSourceWindow(aura::Window* drag_source_window) {
    drag_drop_controller_->drag_source_window_ = drag_source_window;
    drag_source_window->AddObserver(drag_drop_controller_.get());
  }

  gfx::ImageSkia GetDragImage() {
    return static_cast<DragImageView*>(
               drag_drop_controller_->drag_image_widget_->GetContentsView())
        ->GetImage();
  }

  aura::Window* GetDragImageWindow() {
    return drag_drop_controller_->drag_image_widget_
               ? drag_drop_controller_->drag_image_widget_->GetNativeWindow()
               : nullptr;
  }

  MockShellDelegate* mock_shell_delegate() { return mock_shell_delegate_; }

  MockNewWindowDelegate& new_window_delegate() { return new_window_delegate_; }

  gfx::LinearAnimation* cancel_animation() {
    return drag_drop_controller_->cancel_animation_.get();
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
    views::Widget::InitParams params(
        views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.context = GetContext();
    widget->Init(std::move(params));
    widget->Show();
    return widget;
  }

  std::unique_ptr<TestDragDropController> drag_drop_controller_;
  raw_ptr<NiceMock<MockShellDelegate>, DanglingUntriaged> mock_shell_delegate_ =
      nullptr;

  NiceMock<MockNewWindowDelegate> new_window_delegate_;

  bool quit_ = false;

  void RunWithClosure(base::RepeatingCallback<void(bool)> loop) {
    quit_ = false;

    drag_drop_controller_->SetLoopClosureForTesting(
        base::BindLambdaForTesting([&]() { loop.Run(/*inside=*/true); }),
        base::BindLambdaForTesting([&]() { quit_ = true; }));
    while (!quit_) {
      loop.Run(/*inside=*/false);
    }
  }
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
    generator.MoveMouseBy(0, 1);
  }

  generator.ReleaseLeftButton();

  EXPECT_TRUE(drag_drop_controller_->drag_start_received_);
  EXPECT_EQ(num_drags - 1 - drag_view->VerticalDragThreshold(),
            drag_drop_controller_->num_drag_updates_);
  EXPECT_TRUE(drag_drop_controller_->drop_received_);
  EXPECT_EQ(u"I am being dragged", drag_drop_controller_->drag_string_);

  EXPECT_EQ(1, drag_view->num_drag_enters_);
  EXPECT_EQ(num_drags - 1 - drag_view->VerticalDragThreshold(),
            drag_view->num_drag_updates_);
  EXPECT_EQ(1, drag_view->num_drops_);
  EXPECT_EQ(0, drag_view->num_drag_exits_);
  EXPECT_TRUE(drag_view->drag_done_received_);
}

TEST_F(DragDropControllerTest, DragDropMouseReleasesWindowCapture) {
  std::unique_ptr<views::Widget> widget = CreateFramelessWidget();
  DragTestView* drag_view = new DragTestView;
  AddViewToWidgetAndResize(widget.get(), drag_view);
  aura::Window* window = widget->GetNativeView();

  EXPECT_FALSE(window->HasCapture());

  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(), window);

  generator.PressLeftButton();

  // aura::Window does not explicitly take capture, so call this manually to
  // simulate dragging a view which does take capture.
  window->SetCapture();
  EXPECT_TRUE(window->HasCapture());

  int n = 0;
  auto loop_task = [&](bool inside) {
    generator.MoveMouseBy(0, 1);
    n++;

    if (n == 17)
      generator.ReleaseLeftButton();
  };
  RunWithClosure(base::BindLambdaForTesting(loop_task));

  EXPECT_TRUE(drag_view->drag_done_received_);
  EXPECT_FALSE(window->HasCapture());
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
    generator.MoveMouseBy(0, 1);
  }

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
    generator.MoveMouseBy(1, 0);
  }

  generator.ReleaseLeftButton();

  EXPECT_TRUE(drag_drop_controller_->drag_start_received_);
  EXPECT_EQ(num_drags - 1 - drag_view1->HorizontalDragThreshold(),
            drag_drop_controller_->num_drag_updates_);
  EXPECT_TRUE(drag_drop_controller_->drop_received_);
  EXPECT_EQ(u"I am being dragged", drag_drop_controller_->drag_string_);

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
    generator.MoveMouseBy(1, 0);
  }

  generator.ReleaseLeftButton();

  EXPECT_TRUE(drag_drop_controller_->drag_start_received_);
  EXPECT_EQ(num_drags - 1 - drag_view1->HorizontalDragThreshold(),
            drag_drop_controller_->num_drag_updates_);
  EXPECT_TRUE(drag_drop_controller_->drop_received_);
  EXPECT_EQ(u"I am being dragged", drag_drop_controller_->drag_string_);

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
    generator.MoveMouseBy(0, 1);
  }

  drag_view->parent()->RemoveChildView(drag_view.get());
  // View has been removed. We will not get any of the following drag updates.
  int num_drags_2 = 23;
  for (int i = 0; i < num_drags_2; ++i) {
    generator.MoveMouseBy(0, 1);
  }

  generator.ReleaseLeftButton();

  EXPECT_TRUE(drag_drop_controller_->drag_start_received_);
  EXPECT_EQ(num_drags_1 + num_drags_2 - 1 - drag_view->VerticalDragThreshold(),
            drag_drop_controller_->num_drag_updates_);
  EXPECT_TRUE(drag_drop_controller_->drop_received_);
  EXPECT_EQ(u"I am being dragged", drag_drop_controller_->drag_string_);

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
  EXPECT_TRUE(cb->IsFormatAvailable(ui::ClipboardFormatType::PlainTextType(),
                                    ui::ClipboardBuffer::kCopyPaste,
                                    /* data_dst = */ nullptr));

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
  EXPECT_TRUE(cb->IsFormatAvailable(ui::ClipboardFormatType::PlainTextType(),
                                    ui::ClipboardBuffer::kCopyPaste,
                                    /* data_dst = */ nullptr));
  cb->ReadAsciiText(ui::ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr,
                    &result);
  EXPECT_EQ(clip_str, result);
  // Destroy the clipboard here because ash doesn't delete it.
  // crbug.com/158150.
  ui::Clipboard::DestroyClipboardForCurrentThread();
}

// Cancelling followed by closing window should not cause use after free.
// This happens when closing a browser window while dragging.
// crbug.com/1282480
TEST_F(DragDropControllerTest, DragCanceledThenWindowDestroyedDuringDragDrop) {
  std::unique_ptr<views::Widget> widget = CreateFramelessWidget();
  DragTestView* drag_view = new DragTestView;
  AddViewToWidgetAndResize(widget.get(), drag_view);
  aura::Window* window = widget->GetNativeView();

  EventTargetTestDelegate delegate(window);
  aura::client::SetDragDropDelegate(window, &delegate);

  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     widget->GetNativeView());
  generator.PressLeftButton();

  int n = 0;
  bool dragged = false;
  auto loop_task = [&](bool inside) {
    generator.MoveMouseBy(0, 1);
    base::RunLoop().RunUntilIdle();
    n++;

    if (inside && window) {
      dragged = true;
      EXPECT_EQ(window, GetDragWindow());
      EXPECT_GT(n, drag_view->VerticalDragThreshold());
    }

    if (n == 18) {
      drag_drop_controller_->DragCancel();
      widget->CloseNow();
      EXPECT_FALSE(this->GetDragWindow());
      window = nullptr;
    }
  };
  RunWithClosure(base::BindLambdaForTesting(loop_task));
  EXPECT_TRUE(dragged);
  EXPECT_FALSE(GetDragWindow());
  generator.ReleaseLeftButton();

  EXPECT_TRUE(drag_drop_controller_->drag_start_received_);
  // Drag must have been canceled.
  EXPECT_TRUE(drag_drop_controller_->drag_canceled_);
  EXPECT_FALSE(drag_drop_controller_->drop_received_);

  EXPECT_EQ(EventTargetTestDelegate::State::kDragExitInvoked, delegate.state());
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
    generator.MoveMouseBy(0, 1);

    // We send a unexpected mouse move event. Note that we cannot use
    // EventGenerator since it implicitly turns these into mouse drag events.
    // The DragDropController should simply ignore these events.
    gfx::Point mouse_move_location = drag_view->bounds().CenterPoint();
    ui::MouseEvent mouse_move(ui::EventType::kMouseMoved, mouse_move_location,
                              mouse_move_location, ui::EventTimeForNow(), 0, 0);
    ui::EventDispatchDetails details = Shell::GetPrimaryRootWindow()
                                           ->GetHost()
                                           ->GetEventSink()
                                           ->OnEventFromSource(&mouse_move);
    ASSERT_FALSE(details.dispatcher_destroyed);
  }

  generator.ReleaseLeftButton();

  EXPECT_TRUE(drag_drop_controller_->drag_start_received_);
  EXPECT_EQ(num_drags - 1 - drag_view->VerticalDragThreshold(),
            drag_drop_controller_->num_drag_updates_);
  EXPECT_TRUE(drag_drop_controller_->drop_received_);
  EXPECT_EQ(u"I am being dragged", drag_drop_controller_->drag_string_);

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
    generator.MoveMouseBy(0, 1);
  }

  generator.PressKey(ui::VKEY_ESCAPE, 0);

  EXPECT_TRUE(drag_drop_controller_->drag_start_received_);
  EXPECT_EQ(num_drags - 1 - drag_view->VerticalDragThreshold(),
            drag_drop_controller_->num_drag_updates_);
  EXPECT_FALSE(drag_drop_controller_->drop_received_);
  EXPECT_TRUE(drag_drop_controller_->drag_canceled_);
  EXPECT_EQ(u"I am being dragged", drag_drop_controller_->drag_string_);

  EXPECT_EQ(1, drag_view->num_drag_enters_);
  EXPECT_EQ(num_drags - 1 - drag_view->VerticalDragThreshold(),
            drag_view->num_drag_updates_);
  EXPECT_EQ(0, drag_view->num_drops_);
  EXPECT_EQ(1, drag_view->num_drag_exits_);
  EXPECT_TRUE(drag_view->drag_done_received_);
}

TEST_F(DragDropControllerTest, CaptureLostDoesNotCancelsDragDrop) {
  std::unique_ptr<views::Widget> widget = CreateFramelessWidget();
  DragTestView* drag_view = new DragTestView;
  AddViewToWidgetAndResize(widget.get(), drag_view);
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     widget->GetNativeView());
  generator.PressLeftButton();

  int num_drags = 17;
  for (int i = 0; i < num_drags; ++i) {
    generator.MoveMouseBy(0, 1);
  }

  aura::client::GetCaptureClient(widget->GetNativeView()->GetRootWindow())
      ->SetCapture(nullptr);

  EXPECT_TRUE(drag_drop_controller_->drag_start_received_);
  EXPECT_EQ(num_drags - 1 - drag_view->VerticalDragThreshold(),
            drag_drop_controller_->num_drag_updates_);
  EXPECT_FALSE(drag_drop_controller_->drop_received_);
  EXPECT_FALSE(drag_drop_controller_->drag_canceled_);
  EXPECT_EQ(u"I am being dragged", drag_drop_controller_->drag_string_);

  EXPECT_EQ(1, drag_view->num_drag_enters_);
  EXPECT_EQ(num_drags - 1 - drag_view->VerticalDragThreshold(),
            drag_view->num_drag_updates_);
  EXPECT_EQ(0, drag_view->num_drops_);
  EXPECT_EQ(0, drag_view->num_drag_exits_);
  EXPECT_TRUE(drag_view->drag_done_received_);
}

TEST_F(DragDropControllerTest, TouchDragDropInMultipleWindows) {
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
  DispatchGesture(ui::EventType::kGestureLongPress, point);
  gfx::Point gesture_location = point;
  int num_drags = drag_view1->width();
  for (int i = 0; i < num_drags; ++i) {
    gesture_location.Offset(1, 0);
    DispatchGesture(ui::EventType::kGestureScrollUpdate, gesture_location);
  }

  DispatchGesture(ui::EventType::kGestureScrollEnd, gesture_location);

  EXPECT_TRUE(drag_drop_controller_->drag_start_received_);
  EXPECT_EQ(num_drags, drag_drop_controller_->num_drag_updates_);
  EXPECT_TRUE(drag_drop_controller_->drop_received_);
  EXPECT_EQ(u"I am being dragged", drag_drop_controller_->drag_string_);

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
  }

  generator.ReleaseLeftButton();

  EXPECT_GT(icon_replacements, 0);

  EXPECT_TRUE(drag_drop_controller_->drag_start_received_);
  EXPECT_EQ(num_drags - 1 - drag_view1->HorizontalDragThreshold(),
            drag_drop_controller_->num_drag_updates_);
  EXPECT_TRUE(drag_drop_controller_->drop_received_);
  EXPECT_EQ(u"I am being dragged", drag_drop_controller_->drag_string_);

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
  UpdateDisplay("500x400,500x400");
  aura::Window::Windows root_windows = Shell::Get()->GetAllRootWindows();
  for (aura::Window::Windows::iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter) {
    aura::client::SetDragDropClient(*iter, drag_drop_controller_.get());
  }

  {
    auto data = CreateDragData(/*with_image=*/true);
    std::unique_ptr<views::Widget> widget = CreateFramelessWidget();
    aura::Window* window = widget->GetNativeWindow();
    drag_drop_controller_->StartDragAndDrop(
        std::move(data), window->GetRootWindow(), window, gfx::Point(5, 5),
        ui::DragDropTypes::DRAG_MOVE, ui::mojom::DragEventSource::kMouse);

    DragImageWindowObserver observer;
    ASSERT_TRUE(GetDragImageWindow());
    GetDragImageWindow()->AddObserver(&observer);

    {
      ui::MouseEvent e(ui::EventType::kMouseDragged, gfx::Point(200, 0),
                       gfx::Point(200, 0), ui::EventTimeForNow(), ui::EF_NONE,
                       ui::EF_NONE);
      drag_drop_controller_->DragUpdate(window, e);
    }
    {
      ui::MouseEvent e(ui::EventType::kMouseDragged, gfx::Point(600, 0),
                       gfx::Point(600, 0), ui::EventTimeForNow(), ui::EF_NONE,
                       ui::EF_NONE);
      drag_drop_controller_->DragUpdate(window, e);
    }

    drag_drop_controller_->DragCancel();
    CompleteCancelAnimation();
    // Make sure all pending tasks complete to finish cancellation.
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ("5,5", observer.window_location_on_destroying().ToString());
  }

  {
    auto data = CreateDragData(/*with_image=*/true);
    std::unique_ptr<views::Widget> widget = CreateFramelessWidget();
    aura::Window* window = widget->GetNativeWindow();
    drag_drop_controller_->StartDragAndDrop(
        std::move(data), window->GetRootWindow(), window, gfx::Point(405, 405),
        ui::DragDropTypes::DRAG_MOVE, ui::mojom::DragEventSource::kMouse);
    DragImageWindowObserver observer;
    ASSERT_TRUE(GetDragImageWindow());
    GetDragImageWindow()->AddObserver(&observer);

    {
      ui::MouseEvent e(ui::EventType::kMouseDragged, gfx::Point(600, 0),
                       gfx::Point(600, 0), ui::EventTimeForNow(), ui::EF_NONE,
                       ui::EF_NONE);
      drag_drop_controller_->DragUpdate(window, e);
    }
    {
      ui::MouseEvent e(ui::EventType::kMouseDragged, gfx::Point(200, 0),
                       gfx::Point(200, 0), ui::EventTimeForNow(), ui::EF_NONE,
                       ui::EF_NONE);
      drag_drop_controller_->DragUpdate(window, e);
    }

    drag_drop_controller_->DragCancel();
    CompleteCancelAnimation();
    // Make sure all pending tasks complete to finish cancellation.
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ("405,405", observer.window_location_on_destroying().ToString());
  }
  for (aura::Window::Windows::iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter) {
    aura::client::SetDragDropClient(*iter, NULL);
  }
}

// Verifies that a drag is aborted if a display is disconnected during the drag.
TEST_F(DragDropControllerTest, DragCancelOnDisplayDisconnect) {
  UpdateDisplay("500x400,500x400");
  for (aura::Window* root : Shell::Get()->GetAllRootWindows()) {
    aura::client::SetDragDropClient(root, drag_drop_controller_.get());
  }

  auto data = CreateDragData(/*with_image=*/false);
  std::unique_ptr<views::Widget> widget = CreateFramelessWidget();
  aura::Window* window = widget->GetNativeWindow();
  drag_drop_controller_->StartDragAndDrop(
      std::move(data), window->GetRootWindow(), window, gfx::Point(5, 5),
      ui::DragDropTypes::DRAG_MOVE, ui::mojom::DragEventSource::kMouse);

  // Start dragging.
  ui::MouseEvent e1(ui::EventType::kMouseDragged, gfx::Point(200, 0),
                    gfx::Point(200, 0), ui::EventTimeForNow(), ui::EF_NONE,
                    ui::EF_NONE);
  drag_drop_controller_->DragUpdate(window, e1);
  EXPECT_TRUE(drag_drop_controller_->drag_start_received_);
  EXPECT_TRUE(drag_drop_controller_->IsDragDropInProgress());

  // Drag onto the secondary display.
  ui::MouseEvent e2(ui::EventType::kMouseDragged, gfx::Point(600, 0),
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
  ui::TouchEvent press(ui::EventType::kTouchPressed, start, timestamp,
                       ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  generator.Dispatch(&press);

  DispatchGesture(ui::EventType::kGestureLongPress, start);
  timestamp += base::Milliseconds(10);
  ui::TouchEvent move1(ui::EventType::kTouchMoved, mid, timestamp,
                       ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  generator.Dispatch(&move1);
  // Doing two moves instead of one will guarantee to generate a fling at the
  // end.
  timestamp += base::Milliseconds(10);
  ui::TouchEvent move2(ui::EventType::kTouchMoved, end, timestamp,
                       ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  generator.Dispatch(&move2);
  ui::TouchEvent release(ui::EventType::kTouchReleased, end, timestamp,
                         ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  generator.Dispatch(&release);

  EXPECT_TRUE(drag_drop_controller_->drag_start_received_);
  EXPECT_FALSE(drag_drop_controller_->drag_canceled_);
  EXPECT_EQ(2, drag_drop_controller_->num_drag_updates_);
  EXPECT_TRUE(drag_drop_controller_->drop_received_);
  EXPECT_EQ(u"I am being dragged", drag_drop_controller_->drag_string_);
  EXPECT_EQ(1, drag_view->num_drag_enters_);
  EXPECT_EQ(2, drag_view->num_drag_updates_);
  EXPECT_EQ(1, drag_view->num_drops_);
  EXPECT_EQ(0, drag_view->num_drag_exits_);
  EXPECT_TRUE(drag_view->drag_done_received_);
}

TEST_F(DragDropControllerTest, DragObserverEvents) {
  testing::StrictMock<MockDragDropObserver> observer(
      drag_drop_controller_.get());

  {
    auto data = CreateDragData(/*with_image=*/false);
    ui::OSExchangeData* data_ptr = data.get();

    std::unique_ptr<views::Widget> widget = CreateFramelessWidget();
    aura::Window* window = widget->GetNativeWindow();

    EXPECT_CALL(observer, OnDragStarted);
    drag_drop_controller_->StartDragAndDrop(
        std::move(data), window->GetRootWindow(), window, gfx::Point(5, 5),
        ui::DragDropTypes::DRAG_MOVE, ui::mojom::DragEventSource::kMouse);
    testing::Mock::VerifyAndClearExpectations(&observer);

    ui::MouseEvent e(ui::EventType::kMouseDragged, gfx::Point(200, 0),
                     gfx::Point(200, 0), ui::EventTimeForNow(), ui::EF_NONE,
                     ui::EF_NONE);

    {
      testing::InSequence sequence;
      EXPECT_CALL(observer, OnDragUpdated)
          .WillOnce(testing::Invoke([&](const ui::DropTargetEvent& event) {
            gfx::Point root_location_in_screen = event.root_location();
            ::wm::ConvertPointToScreen(
                static_cast<aura::Window*>(event.target())->GetRootWindow(),
                &root_location_in_screen);
            EXPECT_EQ(gfx::Point(200, 0), root_location_in_screen);
            EXPECT_EQ(&event.data(), data_ptr);
          }));
      EXPECT_CALL(observer, OnDragCompleted);
      EXPECT_CALL(observer, OnDropCompleted);
    }

    drag_drop_controller_->Drop(window, e);
    testing::Mock::VerifyAndClearExpectations(&observer);
  }

  drag_drop_controller_->RemoveObserver(&observer);
}

TEST_F(DragDropControllerTest, SetEnabled) {
  TestObserver observer;
  drag_drop_controller_->AddObserver(&observer);

  // Data for the drag.
  auto data = CreateDragData(/*with_image=*/false);
  std::unique_ptr<views::Widget> widget = CreateFramelessWidget();
  aura::Window* window = widget->GetNativeWindow();

  // Cannot start a drag when the controller is disabled.
  drag_drop_controller_->set_enabled(false);
  drag_drop_controller_->StartDragAndDrop(
      std::move(data), window->GetRootWindow(), window, gfx::Point(5, 5),
      ui::DragDropTypes::DRAG_MOVE, ui::mojom::DragEventSource::kMouse);
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
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ui::test::EventGenerator::MoveMouseBy,
                                base::Unretained(&generator), 0, 1));
  // For drag update
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ui::test::EventGenerator::MoveMouseBy,
                                base::Unretained(&generator), 0, 1));
  // For perform drop
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ui::test::EventGenerator::ReleaseLeftButton,
                                base::Unretained(&generator)));

  drag_drop_controller_->set_should_block_during_drag_drop(true);
  auto data = CreateDragData(/*with_image=*/false);
  drag_drop_controller_->StartDragAndDrop(
      std::move(data), window->GetRootWindow(), window.get(), gfx::Point(5, 5),
      ui::DragDropTypes::DRAG_MOVE, ui::mojom::DragEventSource::kMouse);

  EXPECT_EQ(EventTargetTestDelegate::State::kPerformDropInvoked,
            delegate.state());
  base::RunLoop().RunUntilIdle();
}

// Verifies that a tab drag changes the drag operation to a move.
TEST_F(DragDropControllerTest, DragTabChangesDragOperationToMove) {
  EXPECT_CALL(*mock_shell_delegate(), IsTabDrag(_))
      .Times(1)
      .WillOnce(Return(true));
  std::unique_ptr<aura::Window> new_window = CreateToplevelTestWindow();
  EXPECT_CALL(new_window_delegate(), NewWindowForDetachingTab(_, _, _))
      .Times(1)
      .WillOnce(RunOnceCallback<2>(new_window.get()));

  std::unique_ptr<views::Widget> widget = CreateFramelessWidget();
  aura::Window* window = widget->GetNativeWindow();

  // Posted task will be run when the inner loop runs in StartDragAndDrop.
  ui::test::EventGenerator generator(window->GetRootWindow(), window);
  generator.PressLeftButton();
  // For drag enter.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ui::test::EventGenerator::MoveMouseBy,
                                base::Unretained(&generator), 0, 1));
  // For perform drop.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ui::test::EventGenerator::ReleaseLeftButton,
                                base::Unretained(&generator)));

  drag_drop_controller_->set_should_block_during_drag_drop(true);
  DragOperation operation = drag_drop_controller_->StartDragAndDrop(
      std::make_unique<ui::OSExchangeData>(), window->GetRootWindow(), window,
      gfx::Point(5, 5), ui::DragDropTypes::DRAG_NONE,
      ui::mojom::DragEventSource::kMouse);

  EXPECT_EQ(operation, DragOperation::kMove);
}

// Verifies that a tab drag does not crash (UAF) on source window destruction.
TEST_F(DragDropControllerTest, DragTabDoesNotCrashOnSourceWindowDestruction) {
  EXPECT_CALL(*mock_shell_delegate(), IsTabDrag(_))
      .Times(1)
      .WillOnce(Return(true));

  std::unique_ptr<views::Widget> widget = CreateFramelessWidget();
  aura::Window* window = widget->GetNativeWindow();

  // Posted task will be run when the inner loop runs in StartDragAndDrop.
  ui::test::EventGenerator generator(window->GetRootWindow(), window);
  generator.PressLeftButton();

  int step = 0;
  drag_drop_controller_->SetLoopClosureForTesting(
      base::BindLambdaForTesting([&]() {
        switch (step++) {
          case 0:
            // For drag enter.
            generator.MoveMouseBy(0, 1);
            break;
          case 1:
            // Forces a |TabDragDropDelegate::source_window_| destruction.
            widget.reset();
            break;
          case 2:
            // For perform more drag and drop.
            generator.ReleaseLeftButton();
            break;
          default:
            NOTREACHED();
        }
      }),
      base::DoNothing());

  DragOperation operation = drag_drop_controller_->StartDragAndDrop(
      std::make_unique<ui::OSExchangeData>(), window->GetRootWindow(), window,
      gfx::Point(5, 5), ui::DragDropTypes::DRAG_NONE,
      ui::mojom::DragEventSource::kMouse);

  EXPECT_EQ(step, 3);
  EXPECT_EQ(operation, DragOperation::kNone);
}

TEST_F(DragDropControllerTest, ToplevelWindowDragDelegate) {
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate(), -1,
      gfx::Rect(0, 0, 100, 100)));

  // Emulate a full drag and drop flow and verify that toplevel window drag
  // delegate gets notified about the events as expected.
  {
    TestToplevelWindowDragDelegate delegate;
    drag_drop_controller_->set_toplevel_window_drag_delegate(&delegate);

    ui::test::EventGenerator generator(window->GetRootWindow(), window.get());
    generator.PressLeftButton();

    auto data(std::make_unique<ui::OSExchangeData>());
    drag_drop_controller_->StartDragAndDrop(
        std::move(data), window->GetRootWindow(), window.get(),
        gfx::Point(5, 5), ui::DragDropTypes::DRAG_MOVE,
        ui::mojom::DragEventSource::kMouse);

    EXPECT_EQ(TestToplevelWindowDragDelegate::State::kDragStartedInvoked,
              delegate.state());
    EXPECT_EQ(ui::mojom::DragEventSource::kMouse, delegate.source());
    EXPECT_TRUE(delegate.current_location().has_value());
    EXPECT_EQ(gfx::PointF(5, 5), *delegate.current_location());
    EXPECT_EQ(0, delegate.events_forwarded());

    generator.MoveMouseBy(1, 1);
    generator.MoveMouseBy(1, 1);
    generator.MoveMouseBy(1, 1);
    generator.MoveMouseBy(1, 1);
    generator.ReleaseLeftButton();

    EXPECT_EQ(TestToplevelWindowDragDelegate::State::kDragDroppedInvoked,
              delegate.state());
    EXPECT_TRUE(delegate.current_location().has_value());
    EXPECT_EQ(gfx::PointF(54, 54), *delegate.current_location());
    EXPECT_EQ(5, delegate.events_forwarded());
  }

  // Emulate a drag session cancellation and verify the toplevel window drag
  // delegate gets notified about the events as expected.
  {
    TestToplevelWindowDragDelegate delegate;
    drag_drop_controller_->set_toplevel_window_drag_delegate(&delegate);

    ui::test::EventGenerator generator(window->GetRootWindow(), window.get());
    generator.PressLeftButton();

    auto data(std::make_unique<ui::OSExchangeData>());
    drag_drop_controller_->StartDragAndDrop(
        std::move(data), window->GetRootWindow(), window.get(),
        gfx::Point(5, 5), ui::DragDropTypes::DRAG_MOVE,
        ui::mojom::DragEventSource::kMouse);

    EXPECT_EQ(TestToplevelWindowDragDelegate::State::kDragStartedInvoked,
              delegate.state());
    EXPECT_EQ(ui::mojom::DragEventSource::kMouse, delegate.source());
    EXPECT_TRUE(delegate.current_location().has_value());
    EXPECT_EQ(gfx::PointF(5, 5), *delegate.current_location());
    EXPECT_EQ(0, delegate.events_forwarded());

    generator.MoveMouseBy(1, 1);
    generator.MoveMouseBy(1, 1);
    generator.PressKey(ui::VKEY_ESCAPE, 0);

    EXPECT_EQ(TestToplevelWindowDragDelegate::State::kDragCancelledInvoked,
              delegate.state());
    EXPECT_TRUE(delegate.current_location().has_value());
    EXPECT_EQ(gfx::PointF(52, 52), *delegate.current_location());
    EXPECT_EQ(2, delegate.events_forwarded());
  }

  // Regression test for https://crbug.com/1280128.
  // With 2 side-by-side displays, ensures that, when ext-dragging a toplevel
  // window from the rightmost display, entering in the leftmost display results
  // in correct mouse (drag update) events being sent over to toplevel window
  // drag delegate, i.e: negative 'x' coordinates.
  {
    UpdateDisplay("800x600,800x600");
    aura::Window::Windows root_windows = Shell::GetAllRootWindows();
    ASSERT_EQ(2u, root_windows.size());

    aura::client::SetDragDropClient(root_windows[0],
                                    drag_drop_controller_.get());
    aura::client::SetDragDropClient(root_windows[1],
                                    drag_drop_controller_.get());

    const gfx::Rect bounds_within_root1(0, 0, 800, 600);
    const gfx::Rect bounds_within_root2(800, 0, 800, 600);
    std::unique_ptr<aura::Window> window1 =
        CreateTestWindow(bounds_within_root1);
    std::unique_ptr<aura::Window> window2 =
        CreateTestWindow(bounds_within_root2);
    ASSERT_EQ(root_windows[0], window1->GetRootWindow());
    ASSERT_EQ(root_windows[1], window2->GetRootWindow());

    TestToplevelWindowDragDelegate delegate;
    drag_drop_controller_->set_toplevel_window_drag_delegate(&delegate);

    // Press and hold left mouse button at (0,0) in the rightmost display.
    ui::test::EventGenerator generator(window2->GetRootWindow(), {0, 0});
    generator.PressLeftButton();

    auto data(std::make_unique<ui::OSExchangeData>());
    drag_drop_controller_->StartDragAndDrop(
        std::move(data), window2->GetRootWindow(), window2.get(),
        gfx::Point(5, 5), ui::DragDropTypes::DRAG_MOVE,
        ui::mojom::DragEventSource::kMouse);

    EXPECT_EQ(TestToplevelWindowDragDelegate::State::kDragStartedInvoked,
              delegate.state());
    EXPECT_EQ(ui::mojom::DragEventSource::kMouse, delegate.source());
    EXPECT_TRUE(delegate.current_location().has_value());
    EXPECT_EQ(gfx::PointF(5, 5), *delegate.current_location());
    EXPECT_EQ(0, delegate.events_forwarded());

    // Drag to (790,0) in the root window corresponding to the leftmost display,
    // and ensure that the latest drag event received by toplevel window drag
    // delegate targets (-10, 0) location.
    generator.SetTargetWindow(root_windows[0]);
    generator.MoveMouseTo(gfx::Point(790, 0));

    EXPECT_TRUE(delegate.current_location().has_value());
    EXPECT_EQ(gfx::PointF(-10, 0), *delegate.current_location());
    EXPECT_EQ(1, delegate.events_forwarded());

    generator.ReleaseLeftButton();
    EXPECT_EQ(TestToplevelWindowDragDelegate::State::kDragDroppedInvoked,
              delegate.state());
    EXPECT_EQ(2, delegate.events_forwarded());
  }
}

TEST_F(DragDropControllerTest, ToplevelWindowDragDelegateWithTouch) {
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate(), -1,
      gfx::Rect(0, 0, 100, 100)));

  // Emulate a full drag and drop flow and verify that toplevel window drag
  // delegate gets notified about the events as expected.
  TestToplevelWindowDragDelegate delegate;
  drag_drop_controller_->set_toplevel_window_drag_delegate(&delegate);

  auto data(std::make_unique<ui::OSExchangeData>());
  drag_drop_controller_->StartDragAndDrop(
      std::move(data), window->GetRootWindow(), window.get(), gfx::Point(5, 5),
      ui::DragDropTypes::DRAG_MOVE, ui::mojom::DragEventSource::kTouch);

  EXPECT_EQ(TestToplevelWindowDragDelegate::State::kDragStartedInvoked,
            delegate.state());
  EXPECT_EQ(ui::mojom::DragEventSource::kTouch, delegate.source());
  EXPECT_TRUE(delegate.current_location().has_value());
  EXPECT_EQ(gfx::PointF(5, 5), *delegate.current_location());
  EXPECT_EQ(0, delegate.events_forwarded());

  drag_drop_controller_->DragCancel();
}

TEST_F(DragDropControllerTest, ToplevelWindowDragDelegateWithTouch2) {
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate(), -1,
      gfx::Rect(0, 0, 100, 100)));

  // Emulate a full drag and drop flow with touch and verify that toplevel
  // window drag delegate gets notified about the events as expected.
  TestToplevelWindowDragDelegate delegate;
  drag_drop_controller_->set_toplevel_window_drag_delegate(&delegate);

  ui::test::EventGenerator generator(window->GetRootWindow(), window.get());
  generator.PressTouch();
  auto point = gfx::Point(5, 5);

  auto data(std::make_unique<ui::OSExchangeData>());
  drag_drop_controller_->StartDragAndDrop(
      std::move(data), window->GetRootWindow(), window.get(), point,
      ui::DragDropTypes::DRAG_MOVE, ui::mojom::DragEventSource::kTouch);

  EXPECT_EQ(TestToplevelWindowDragDelegate::State::kDragStartedInvoked,
            delegate.state());
  EXPECT_EQ(ui::mojom::DragEventSource::kTouch, delegate.source());
  EXPECT_TRUE(delegate.current_location().has_value());
  EXPECT_EQ(gfx::PointF(point), *delegate.current_location());
  EXPECT_EQ(0, delegate.events_forwarded());

  gfx::Point gesture_location = point;
  int num_drags = 5;
  for (int i = 0; i < num_drags; ++i) {
    gesture_location.Offset(1, 1);
    DispatchGesture(ui::EventType::kGestureScrollUpdate, gesture_location);
    EXPECT_EQ(i + 1, delegate.events_forwarded());
  }

  DispatchGesture(ui::EventType::kGestureScrollEnd, gesture_location);
  EXPECT_EQ(TestToplevelWindowDragDelegate::State::kDragDroppedInvoked,
            delegate.state());
  EXPECT_TRUE(delegate.current_location().has_value());
  EXPECT_EQ(gfx::PointF(10, 10), *delegate.current_location());
  EXPECT_EQ(6, delegate.events_forwarded());
}

TEST_F(DragDropControllerTest, DragWithChromeTabDelegateTakesCapture) {
  EXPECT_CALL(*mock_shell_delegate(), IsTabDrag(_))
      .Times(1)
      .WillOnce(Return(true));

  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate(), -1,
      gfx::Rect(0, 0, 100, 100)));

  auto data = CreateDragData(/*with_image=*/true);

  drag_drop_controller_->StartDragAndDrop(
      std::move(data), window->GetRootWindow(), window.get(), gfx::Point(5, 5),
      ui::DragDropTypes::DRAG_MOVE, ui::mojom::DragEventSource::kTouch);

  // Should create a captue delegate which takes capture from the window.
  EXPECT_TRUE(drag_drop_controller_->get_capture_delegate());

  drag_drop_controller_.reset();
}

// Regression test for crbug.com/1297209.
// In tablet mode split view, with the browser tab strip on one side and desks
// overview (or any other window) on the other, touch and hold a desk mini view
// (or that other window) and drag a browser tab simultaneously.
TEST_F(DragDropControllerTest, TabletSplitViewDragTwoBrowserTabs) {
  // Enter tablet mode. Avoid TabletModeController::OnGetSwitchStates() from
  // disabling tablet mode.
  base::RunLoop().RunUntilIdle();
  ash::TabletModeControllerTestApi().EnterTabletMode();

  // Enter tablet split view mode by snapping a tab window on each side.
  // A generic top level window is enough to fake a chrome tab.
  std::unique_ptr<aura::Window> tab_window1 = CreateToplevelTestWindow();
  std::unique_ptr<aura::Window> tab_window2 = CreateToplevelTestWindow();
  SplitViewController* const split_view_controller =
      SplitViewController::Get(tab_window1.get());
  split_view_controller->SnapWindow(tab_window1.get(), SnapPosition::kPrimary);
  split_view_controller->SnapWindow(tab_window2.get(),
                                    SnapPosition::kSecondary);
  EXPECT_TRUE(split_view_controller->InTabletSplitViewMode());

  // Touch and hold the right tab window.
  GetEventGenerator()->PressTouch(
      tab_window2->GetBoundsInScreen().CenterPoint());

  // Prepare to drag the left tab window.
  EXPECT_CALL(*mock_shell_delegate(), IsTabDrag(_)).WillOnce(Return(true));

  // Drag and drop needs a drag image to work.
  auto data = CreateDragData(/*with_image=*/true);

  // Start drag and drop on the left tab window.
  auto drag_operation = drag_drop_controller_->StartDragAndDrop(
      std::move(data), tab_window1->GetRootWindow(), tab_window1.get(),
      tab_window1->GetBoundsInScreen().CenterPoint(),
      ui::DragDropTypes::DRAG_MOVE, ui::mojom::DragEventSource::kTouch);
  EXPECT_EQ(drag_operation, DragOperation::kNone);
  EXPECT_FALSE(drag_drop_controller_->IsDragDropInProgress());
  EXPECT_FALSE(drag_drop_controller_->get_capture_delegate());
  EXPECT_FALSE(tab_window1->HasObserver(drag_drop_controller_.get()));
}

TEST_F(DragDropControllerTest, DragImageWidgetNotCreatedIfNoImage) {
  std::unique_ptr<views::Widget> widget = CreateFramelessWidget();
  aura::Window* window = widget->GetNativeWindow();

  auto data = CreateDragData(/*with_image=*/false);
  drag_drop_controller_->StartDragAndDrop(
      std::move(data), window->GetRootWindow(), window, gfx::Point(5, 5),
      ui::DragDropTypes::DRAG_MOVE, ui::mojom::DragEventSource::kMouse);
  EXPECT_FALSE(GetDragImageWindow());
  drag_drop_controller_->DragCancel();

  data = CreateDragData(/*with_image=*/true);
  drag_drop_controller_->StartDragAndDrop(
      std::move(data), window->GetRootWindow(), window, gfx::Point(5, 5),
      ui::DragDropTypes::DRAG_MOVE, ui::mojom::DragEventSource::kMouse);
  EXPECT_TRUE(GetDragImageWindow());
}

TEST_F(DragDropControllerTest, ObserverNotifiedOfDestruction) {
  NiceMock<MockDragDropObserver> drag_drop_observer(
      drag_drop_controller_.get());
  EXPECT_CALL(drag_drop_observer, OnDragDropClientDestroying).WillOnce([&]() {
    drag_drop_observer.ResetObservation();
  });

  std::unique_ptr<views::Widget> widget = CreateFramelessWidget();
  DragTestView* drag_view = new DragTestView;
  AddViewToWidgetAndResize(widget.get(), drag_view);
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     widget->GetNativeView());
  generator.PressLeftButton();

  int num_drags = 17;
  for (int i = 0; i < num_drags; ++i) {
    generator.MoveMouseBy(0, 1);
  }

  aura::client::SetDragDropClient(Shell::GetPrimaryRootWindow(), NULL);
  drag_drop_controller_.reset();
}

// Verifies drag-and-drop with a data transfer policy controller.
class DragDropControllerDlpTest : public DragDropControllerTest {
 public:
  // DragDropControllerTest:
  void SetUp() override {
    DragDropControllerTest::SetUp();

    window_.reset(CreateTestWindowInShellWithDelegate(
        aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate(),
        /*id=*/-1, gfx::Rect(0, 0, 100, 100)));
    delegate_ = std::make_unique<EventTargetTestDelegate>(window_.get());
    aura::client::SetDragDropDelegate(window_.get(), delegate_.get());
    drag_and_drop_observer_ = std::make_unique<NiceMock<MockDragDropObserver>>(
        drag_drop_controller_.get());
  }

  void TearDown() override {
    drag_and_drop_observer_.reset();
    delegate_.reset();
    window_.reset();

    DragDropControllerTest::TearDown();
  }

  // Performs drag-and-drop on `window_` with the specified drag data. Data drop
  // is allowed or not by `dlp_contoller_`.
  void PerformDlpDragAndDrop(std::unique_ptr<ui::OSExchangeData> drag_data) {
    // Posted task will be run when the inner loop runs in StartDragAndDrop.
    ui::test::EventGenerator generator(window_->GetRootWindow(), window_.get());
    generator.PressLeftButton();

    drag_drop_controller_->StartDragAndDrop(
        std::move(drag_data), window_->GetRootWindow(), window_.get(),
        gfx::Point(5, 5), ui::DragDropTypes::DRAG_MOVE,
        ui::mojom::DragEventSource::kMouse);

    // For drag enter
    generator.MoveMouseBy(0, 1);
    // For drag update
    generator.MoveMouseBy(0, 1);
    // For perform drop
    generator.ReleaseLeftButton();
  }

  // A mock data transfer policy controller. Customized to allow/disallow data
  // drop in tests.
  ui::MockDataTransferPolicyController dlp_contoller_;

  std::unique_ptr<EventTargetTestDelegate> delegate_;

  std::unique_ptr<aura::Window> window_;

  // A mock drag-and-drop observer to verify the API function calling order.
  std::unique_ptr<NiceMock<MockDragDropObserver>> drag_and_drop_observer_;
};

// Tests when drop is allowed synchronously.
TEST_F(DragDropControllerDlpTest, AllowedSyncDragDrop) {
  {
    testing::InSequence s;
    EXPECT_CALL(*drag_and_drop_observer_, OnDragStarted);
    EXPECT_CALL(*drag_and_drop_observer_, OnDragCompleted);
    EXPECT_CALL(*drag_and_drop_observer_,
                OnDropCompleted(ui::mojom::DragOperation::kMove));
  }

  // Configure `dlp_controller_` to allow sync drop.
  EXPECT_CALL(dlp_contoller_, DropIfAllowed(_, _, _, _))
      .WillOnce([&](std::optional<ui::DataTransferEndpoint> data_src,
                    std::optional<ui::DataTransferEndpoint> data_dst,
                    std::optional<std::vector<ui::FileInfo>> filenames,
                    base::OnceClosure drop_cb) { std::move(drop_cb).Run(); });

  PerformDlpDragAndDrop(CreateDragData(/*with_image=*/false));

  EXPECT_EQ(EventTargetTestDelegate::State::kPerformDropInvoked,
            delegate_->state());
}

// Tests when drag is cancelled before drop.
TEST_F(DragDropControllerDlpTest, CancelDragBeforeDrop) {
  // Observers should not be notified of drop completion since the async drop
  // should be interrupted by a new drag-and-drop session.
  EXPECT_CALL(*drag_and_drop_observer_, OnDropCompleted).Times(0);

  {
    testing::InSequence s;
    EXPECT_CALL(*drag_and_drop_observer_, OnDragStarted);
    EXPECT_CALL(*drag_and_drop_observer_, OnDragCancelled);
  }

  // Drag to `window_`.
  ui::test::EventGenerator generator(window_->GetRootWindow(), window_.get());
  generator.PressLeftButton();
  drag_drop_controller_->StartDragAndDrop(
      CreateDragData(/*with_image=*/true), window_->GetRootWindow(),
      window_.get(), gfx::Point(5, 5), ui::DragDropTypes::DRAG_MOVE,
      ui::mojom::DragEventSource::kMouse);
  generator.MoveMouseBy(0, 1);

  // Cancel before drop.
  drag_drop_controller_->DragCancel();
  generator.ReleaseLeftButton();

  // There is a non-empty drag image, an animation is expected to be run for
  // cancellation.
  EXPECT_TRUE(cancel_animation());
  EXPECT_TRUE(GetDragImageWindow());
  EXPECT_EQ(EventTargetTestDelegate::State::kDragExitInvoked,
            delegate_->state());
}

// Tests when drop is allowed asynchronously.
TEST_F(DragDropControllerDlpTest, AllowedAsyncDrop) {
  {
    testing::InSequence s;
    EXPECT_CALL(*drag_and_drop_observer_, OnDragStarted);
    EXPECT_CALL(*drag_and_drop_observer_, OnDragCompleted);
    EXPECT_CALL(*drag_and_drop_observer_, OnDropCompleted);
  }

  // Hold the drop callback passed to `dlp_controller_` then run this drop
  // callback later. It emulates a successful async drop.
  base::OnceClosure drop_callback;
  EXPECT_CALL(dlp_contoller_, DropIfAllowed(_, _, _, _))
      .WillOnce([&](std::optional<ui::DataTransferEndpoint> data_src,
                    std::optional<ui::DataTransferEndpoint> data_dst,
                    std::optional<std::vector<ui::FileInfo>> filenames,
                    base::OnceClosure drop_cb) {
        drop_callback = std::move(drop_cb);
      });

  PerformDlpDragAndDrop(CreateDragData(/*with_image=*/true));
  std::move(drop_callback).Run();

  // Check that there is no drag-and-drop in progress after the async drop.
  EXPECT_FALSE(drag_drop_controller_->IsDragDropInProgress());
}

// Tests when the first drop is allowed after the second drag-and-drop session
// starts.
TEST_F(DragDropControllerDlpTest, InterruptedAsyncDrop) {
  // Since the second drag-and-drop session starts before the first drop is
  // completed, an observer should not be notified of the first drop completion.
  EXPECT_CALL(*drag_and_drop_observer_, OnDropCompleted).Times(0);
  {
    testing::InSequence s;
    EXPECT_CALL(*drag_and_drop_observer_, OnDragStarted);
    EXPECT_CALL(*drag_and_drop_observer_, OnDragCompleted);
    EXPECT_CALL(*drag_and_drop_observer_, OnDragStarted);
  }

  base::OnceClosure drop_callback;
  EXPECT_CALL(dlp_contoller_, DropIfAllowed(_, _, _, _))
      .WillOnce([&](std::optional<ui::DataTransferEndpoint> data_src,
                    std::optional<ui::DataTransferEndpoint> data_dst,
                    std::optional<std::vector<ui::FileInfo>> filenames,
                    base::OnceClosure drop_cb) {
        drop_callback = std::move(drop_cb);
      });

  PerformDlpDragAndDrop(CreateDragData(/*with_image=*/true));
  EXPECT_FALSE(cancel_animation());
  EXPECT_FALSE(GetDragImageWindow());

  auto data = std::make_unique<ui::OSExchangeData>();
  data->SetString(u"I am being dragged 2");
  drag_drop_controller_->StartDragAndDrop(
      std::move(data), window_->GetRootWindow(), window_.get(),
      gfx::Point(5, 5), ui::DragDropTypes::DRAG_MOVE,
      ui::mojom::DragEventSource::kMouse);

  // Run `drop_callback` after the second drag-and-drop starts.
  std::move(drop_callback).Run();

  EXPECT_EQ(EventTargetTestDelegate::State::kDragUpdateInvoked,
            delegate_->state());
}

// Tests when drop is disallowed asyncly.
TEST_F(DragDropControllerDlpTest, DlpDisallowAsyncDrop) {
  {
    testing::InSequence s;
    EXPECT_CALL(*drag_and_drop_observer_, OnDragStarted);
    EXPECT_CALL(*drag_and_drop_observer_, OnDragCompleted);
    EXPECT_CALL(*drag_and_drop_observer_, OnDragCancelled);
  }

  // Hold the drop callback passed to `dlp_controller_`. Because `drop_callback`
  // does not run, it emulates an async disallowed drop.
  base::OnceClosure drop_callback;
  EXPECT_CALL(dlp_contoller_, DropIfAllowed(_, _, _, _))
      .WillOnce([&](std::optional<ui::DataTransferEndpoint> data_src,
                    std::optional<ui::DataTransferEndpoint> data_dst,
                    std::optional<std::vector<ui::FileInfo>> filenames,
                    base::OnceClosure drop_cb) {
        drop_callback = std::move(drop_cb);
      });

  PerformDlpDragAndDrop(CreateDragData(/*with_image=*/true));
}

class MouseOrTouchDragDropControllerTest
    : public DragDropControllerTest,
      public testing::WithParamInterface<bool> {
 public:
  MouseOrTouchDragDropControllerTest() = default;
  MouseOrTouchDragDropControllerTest(
      const MouseOrTouchDragDropControllerTest&) = delete;
  MouseOrTouchDragDropControllerTest& operator=(
      const MouseOrTouchDragDropControllerTest&) = delete;
  ~MouseOrTouchDragDropControllerTest() = default;

  bool IsMouse() const { return GetParam(); }

  void Press(ui::test::EventGenerator& generator) {
    if (IsMouse()) {
      generator.PressLeftButton();
    } else {
      generator.PressTouch();
      // long press requires > 1.15s
      task_environment()->AdvanceClock(base::Seconds(2));
    }
  }
  void MoveBy(ui::test::EventGenerator& generator, int x, int y) {
    if (IsMouse())
      generator.MoveMouseBy(x, y);
    else
      generator.MoveTouchBy(x, y);
  }

  void Release(ui::test::EventGenerator& generator) {
    if (IsMouse())
      generator.ReleaseLeftButton();
    else
      generator.ReleaseTouch();
  }
};

// Ensures that the drag drop continues after window destruction.
TEST_P(MouseOrTouchDragDropControllerTest, WindowDestroyedDuringDragDrop) {
  std::unique_ptr<views::Widget> widget = CreateFramelessWidget();
  DragTestView* drag_view = new DragTestView;
  AddViewToWidgetAndResize(widget.get(), drag_view);
  aura::Window* window = widget->GetNativeView();

  EventTargetTestDelegate delegate(window);
  aura::client::SetDragDropDelegate(window, &delegate);

  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     widget->GetNativeView());
  Press(generator);

  int n = 0;
  bool dragged = false;
  auto loop_task = [&](bool inside) {
    MoveBy(generator, 0, 1);
    base::RunLoop().RunUntilIdle();
    n++;
    if (inside && window) {
      dragged = true;
      if (IsMouse()) {
        EXPECT_EQ(window, GetDragWindow());
        EXPECT_GT(n, drag_view->VerticalDragThreshold());
      } else if (n > 5) {
        EXPECT_EQ(window, GetDragWindow());
      }
    }

    if (n == 18) {
      widget->CloseNow();
      EXPECT_FALSE(this->GetDragWindow());
      window = nullptr;
    }
    if (n == 100)
      Release(generator);
  };
  RunWithClosure(base::BindLambdaForTesting(loop_task));
  EXPECT_TRUE(dragged);
  EXPECT_FALSE(GetDragWindow());
  EXPECT_GT(n, 50);

  EXPECT_TRUE(drag_drop_controller_->drag_start_received_);
  EXPECT_FALSE(drag_drop_controller_->drag_canceled_);
  EXPECT_TRUE(drag_drop_controller_->drop_received_);
  EXPECT_EQ(EventTargetTestDelegate::State::kDragExitInvoked, delegate.state());
}

INSTANTIATE_TEST_SUITE_P(All,
                         MouseOrTouchDragDropControllerTest,
                         testing::Bool());

namespace {

class DragDropControllerLongTapCancelTest : public DragDropControllerTest {
 public:
  DragDropControllerLongTapCancelTest(
      const DragDropControllerLongTapCancelTest& other) = delete;
  DragDropControllerLongTapCancelTest& operator=(
      const DragDropControllerLongTapCancelTest& other) = delete;

 protected:
  DragDropControllerLongTapCancelTest() = default;
  ~DragDropControllerLongTapCancelTest() override = default;

  void SetUp() override {
    DragDropControllerTest::SetUp();

    widget_ = CreateFramelessWidget();
    drag_view_ = new DragTestView;
    AddViewToWidgetAndResize(widget_.get(), drag_view_);
    generator_ = std::make_unique<ui::test::EventGenerator>(
        Shell::GetPrimaryRootWindow(), widget_->GetNativeView());
  }

  void IssueLongTap() {
    auto loop_task = [this](bool inside) {
      gfx::Point point = gfx::Rect(drag_view_->bounds()).CenterPoint();
      if (!inside) {
        generator_->PressTouch();
        DispatchGesture(ui::EventType::kGestureLongPress, point);
      } else {
        ASSERT_FALSE(inside_loop_task_executed_);
        inside_loop_task_executed_ = true;

        EXPECT_FALSE(drag_view_->long_tap_received_);
        DispatchGesture(ui::EventType::kGestureLongTap, point);
      }
    };
    RunWithClosure(base::BindLambdaForTesting(loop_task));
  }

  std::unique_ptr<views::Widget> widget_;
  raw_ptr<DragTestView> drag_view_ = nullptr;
  std::unique_ptr<ui::test::EventGenerator> generator_;
  bool inside_loop_task_executed_ = false;
};

}  // namespace

TEST_F(DragDropControllerLongTapCancelTest, TouchDragDropCancelsOnLongTap) {
  IssueLongTap();

  EXPECT_TRUE(drag_drop_controller_->drag_start_received_);
  EXPECT_TRUE(drag_drop_controller_->drag_canceled_);
  EXPECT_EQ(0, drag_drop_controller_->num_drag_updates_);
  EXPECT_FALSE(drag_drop_controller_->drop_received_);
  EXPECT_EQ(u"I am being dragged", drag_drop_controller_->drag_string_);
  EXPECT_EQ(0, drag_view_->num_drag_enters_);
  EXPECT_EQ(0, drag_view_->num_drops_);
  EXPECT_EQ(0, drag_view_->num_drag_exits_);
  EXPECT_TRUE(drag_view_->drag_done_received_);

  // The long tap gesture is expected to be forwarded after the cancel
  // animation.
  ASSERT_TRUE(cancel_animation());
  EXPECT_FALSE(drag_view_->long_tap_received_);
  CompleteCancelAnimation();
  EXPECT_TRUE(drag_view_->long_tap_received_);
}

TEST_F(DragDropControllerLongTapCancelTest,
       LongTapForwardedWithoutCancelAnimation) {
  drag_view_->OmitDragImage();

  // DragDropController does not support touch drag/drop without a drag image,
  // unless it has a non-null |toplevel_window_drag_delegate_|.
  TestToplevelWindowDragDelegate delegate;
  drag_drop_controller_->set_toplevel_window_drag_delegate(&delegate);

  IssueLongTap();

  ASSERT_FALSE(cancel_animation());
  EXPECT_TRUE(drag_view_->long_tap_received_);
}

}  // namespace ash
