// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/events/peripheral_customization_event_rewriter.h"

#include <algorithm>
#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/public/mojom/input_device_settings.mojom-shared.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/test/ash_test_base.h"
#include "base/containers/flat_map.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/test/test_event_rewriter_continuation.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point_f.h"

namespace ash {

namespace {

constexpr int kDeviceId = 1;

class TestEventRewriterContinuation
    : public ui::test::TestEventRewriterContinuation {
 public:
  TestEventRewriterContinuation() = default;

  TestEventRewriterContinuation(const TestEventRewriterContinuation&) = delete;
  TestEventRewriterContinuation& operator=(
      const TestEventRewriterContinuation&) = delete;

  ~TestEventRewriterContinuation() override = default;

  ui::EventDispatchDetails SendEvent(const ui::Event* event) override {
    passthrough_event = event->Clone();
    return ui::EventDispatchDetails();
  }

  ui::EventDispatchDetails SendEventFinally(const ui::Event* event) override {
    rewritten_event = event->Clone();
    return ui::EventDispatchDetails();
  }

  ui::EventDispatchDetails DiscardEvent() override {
    return ui::EventDispatchDetails();
  }

  void reset() {
    passthrough_event.reset();
    rewritten_event.reset();
  }

  bool discarded() { return !(passthrough_event || rewritten_event); }

  std::unique_ptr<ui::Event> passthrough_event;
  std::unique_ptr<ui::Event> rewritten_event;

  base::WeakPtrFactory<TestEventRewriterContinuation> weak_ptr_factory_{this};
};

class TestObserver : public PeripheralCustomizationEventRewriter::Observer {
 public:
  void OnMouseButtonPressed(int device_id,
                            const mojom::Button& button) override {
    pressed_mouse_buttons_[device_id].push_back(button.Clone());
  }

  void OnGraphicsTabletButtonPressed(int device_id,
                                     const mojom::Button& button) override {
    pressed_graphics_tablet_buttons_[device_id].push_back(button.Clone());
  }

  const base::flat_map<int, std::vector<mojom::ButtonPtr>>&
  pressed_mouse_buttons() const {
    return pressed_mouse_buttons_;
  }

  const base::flat_map<int, std::vector<mojom::ButtonPtr>>&
  pressed_graphics_tablet_buttons() const {
    return pressed_graphics_tablet_buttons_;
  }

 private:
  base::flat_map<int, std::vector<mojom::ButtonPtr>> pressed_mouse_buttons_;
  base::flat_map<int, std::vector<mojom::ButtonPtr>>
      pressed_graphics_tablet_buttons_;
};

struct EventRewriterTestData {
  ui::MouseEvent incoming_event;
  absl::optional<ui::MouseEvent> rewritten_event;
  absl::optional<mojom::Button> pressed_button;

  EventRewriterTestData(ui::MouseEvent incoming_event,
                        absl::optional<ui::MouseEvent> rewritten_event)
      : incoming_event(incoming_event),
        rewritten_event(rewritten_event),
        pressed_button(absl::nullopt) {}

  EventRewriterTestData(ui::MouseEvent incoming_event,
                        absl::optional<ui::MouseEvent> rewritten_event,
                        mojom::CustomizableButton button)
      : incoming_event(incoming_event), rewritten_event(rewritten_event) {
    pressed_button = mojom::Button();
    pressed_button->set_customizable_button(button);
  }

  EventRewriterTestData(ui::MouseEvent incoming_event,
                        absl::optional<ui::MouseEvent> rewritten_event,
                        ui::KeyboardCode key_code)
      : incoming_event(incoming_event), rewritten_event(rewritten_event) {
    pressed_button = mojom::Button();
    pressed_button->set_vkey(key_code);
  }

  EventRewriterTestData(const EventRewriterTestData& data) = default;
};

ui::MouseEvent CreateMouseButtonEvent(ui::EventType type,
                                      int flags,
                                      int changed_button_flags,
                                      int device_id = kDeviceId) {
  ui::MouseEvent mouse_event(type, /*location=*/gfx::PointF{},
                             /*root_location=*/gfx::PointF{},
                             /*time_stamp=*/{}, flags, changed_button_flags);
  mouse_event.set_source_device_id(device_id);
  return mouse_event;
}

std::string ConvertToString(const ui::Event& event) {
  if (event.IsMouseEvent()) {
    const auto& mouse_event = *event.AsMouseEvent();
    return base::StringPrintf(
        "MouseEvent type=%d flags=0x%X changed_button_flags=0x%X",
        mouse_event.type(), mouse_event.flags(),
        mouse_event.changed_button_flags());
  }
  NOTREACHED_NORETURN();
}

}  // namespace

class PeripheralCustomizationEventRewriterTest : public AshTestBase {
 public:
  PeripheralCustomizationEventRewriterTest() = default;
  PeripheralCustomizationEventRewriterTest(
      const PeripheralCustomizationEventRewriterTest&) = delete;
  PeripheralCustomizationEventRewriterTest& operator=(
      const PeripheralCustomizationEventRewriterTest&) = delete;
  ~PeripheralCustomizationEventRewriterTest() override = default;

  // testing::Test:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({features::kPeripheralCustomization,
                                           features::kInputDeviceSettingsSplit},
                                          {});

    AshTestBase::SetUp();
    rewriter_ = std::make_unique<PeripheralCustomizationEventRewriter>();
    observer_ = std::make_unique<TestObserver>();
    rewriter_->AddObserver(observer_.get());
  }

  void TearDown() override {
    rewriter_->RemoveObserver(observer_.get());
    observer_.reset();
    rewriter_.reset();
    AshTestBase::TearDown();
    scoped_feature_list_.Reset();
  }

 protected:
  std::unique_ptr<PeripheralCustomizationEventRewriter> rewriter_;
  std::unique_ptr<TestObserver> observer_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PeripheralCustomizationEventRewriterTest, MouseButtonWithoutObserving) {
  TestEventRewriterContinuation continuation;

  auto back_mouse_event = CreateMouseButtonEvent(
      ui::ET_MOUSE_PRESSED, ui::EF_BACK_MOUSE_BUTTON, ui::EF_BACK_MOUSE_BUTTON);

  rewriter_->RewriteEvent(back_mouse_event,
                          continuation.weak_ptr_factory_.GetWeakPtr());
  ASSERT_TRUE(continuation.passthrough_event);
  ASSERT_TRUE(continuation.passthrough_event->IsMouseEvent());
  EXPECT_EQ(ConvertToString(back_mouse_event),
            ConvertToString(*continuation.passthrough_event));
}

TEST_F(PeripheralCustomizationEventRewriterTest,
       InvalidEventTypeMouseObserving) {
  TestEventRewriterContinuation continuation;

  rewriter_->StartObservingMouse(kDeviceId);

  ui::MouseEvent event =
      CreateMouseButtonEvent(ui::ET_MOUSE_DRAGGED, ui::EF_NONE, ui::EF_NONE);

  rewriter_->RewriteEvent(event, continuation.weak_ptr_factory_.GetWeakPtr());
  ASSERT_TRUE(continuation.passthrough_event);
  ASSERT_TRUE(continuation.passthrough_event->IsMouseEvent());
  EXPECT_EQ(ConvertToString(event),
            ConvertToString(*continuation.passthrough_event));
}

class MouseButtonObserverTest
    : public PeripheralCustomizationEventRewriterTest,
      public testing::WithParamInterface<EventRewriterTestData> {};

INSTANTIATE_TEST_SUITE_P(
    All,
    MouseButtonObserverTest,
    testing::ValuesIn(std::vector<EventRewriterTestData>{
        {
            CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                   ui::EF_BACK_MOUSE_BUTTON,
                                   ui::EF_BACK_MOUSE_BUTTON),
            absl::nullopt,
            mojom::CustomizableButton::kBack,
        },
        {
            CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                   ui::EF_FORWARD_MOUSE_BUTTON,
                                   ui::EF_FORWARD_MOUSE_BUTTON),
            absl::nullopt,
            mojom::CustomizableButton::kForward,
        },
        {
            CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                   ui::EF_MIDDLE_MOUSE_BUTTON,
                                   ui::EF_MIDDLE_MOUSE_BUTTON),
            absl::nullopt,
            mojom::CustomizableButton::kMiddle,
        },
        {
            CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                   ui::EF_MIDDLE_MOUSE_BUTTON |
                                       ui::EF_LEFT_MOUSE_BUTTON,
                                   ui::EF_MIDDLE_MOUSE_BUTTON),
            absl::nullopt,
            mojom::CustomizableButton::kMiddle,
        },

        // Observer notified only when mouse button pressed.
        {
            CreateMouseButtonEvent(ui::ET_MOUSE_RELEASED,
                                   ui::EF_BACK_MOUSE_BUTTON,
                                   ui::EF_BACK_MOUSE_BUTTON),
            /*rewritten_event=*/absl::nullopt,
        },

        // Left click ignored for buttons from a mouse.
        {
            CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                   ui::EF_LEFT_MOUSE_BUTTON,
                                   ui::EF_LEFT_MOUSE_BUTTON),
            CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                   ui::EF_LEFT_MOUSE_BUTTON,
                                   ui::EF_LEFT_MOUSE_BUTTON),
        },

        // Right click ignored for buttons from a mouse.
        {
            CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                   ui::EF_RIGHT_MOUSE_BUTTON,
                                   ui::EF_RIGHT_MOUSE_BUTTON),
            CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                   ui::EF_RIGHT_MOUSE_BUTTON,
                                   ui::EF_RIGHT_MOUSE_BUTTON),
        },

        // Other flags are ignored when included in the event with other
        // buttons.
        {
            CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                   ui::EF_LEFT_MOUSE_BUTTON |
                                       ui::EF_BACK_MOUSE_BUTTON,
                                   ui::EF_LEFT_MOUSE_BUTTON),
            CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                   ui::EF_LEFT_MOUSE_BUTTON,
                                   ui::EF_LEFT_MOUSE_BUTTON),
        },
        {
            CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                   ui::EF_RIGHT_MOUSE_BUTTON |
                                       ui::EF_MIDDLE_MOUSE_BUTTON,
                                   ui::EF_NONE),
            CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                   ui::EF_RIGHT_MOUSE_BUTTON,
                                   ui::EF_NONE),
        },
    }),
    [](const testing::TestParamInfo<EventRewriterTestData>& info) {
      std::string name = ConvertToString(info.param.incoming_event);
      std::replace(name.begin(), name.end(), ' ', '_');
      std::replace(name.begin(), name.end(), '=', '_');
      return name;
    });

TEST_P(MouseButtonObserverTest, EventRewriting) {
  const auto& data = GetParam();

  rewriter_->StartObservingMouse(kDeviceId);

  TestEventRewriterContinuation continuation;
  rewriter_->RewriteEvent(data.incoming_event,
                          continuation.weak_ptr_factory_.GetWeakPtr());
  if (!data.rewritten_event) {
    ASSERT_TRUE(continuation.discarded());
    if (data.pressed_button) {
      const auto& actual_pressed_buttons =
          observer_->pressed_mouse_buttons().at(kDeviceId);
      ASSERT_EQ(1u, actual_pressed_buttons.size());
      EXPECT_EQ(*data.pressed_button, *actual_pressed_buttons[0]);
    }
  } else {
    ASSERT_TRUE(continuation.passthrough_event);
    ASSERT_TRUE(continuation.passthrough_event->IsMouseEvent());
    EXPECT_EQ(ConvertToString(*data.rewritten_event),
              ConvertToString(*continuation.passthrough_event));
  }

  rewriter_->StopObserving();
  continuation.reset();

  // After we stop observing, the passthrough event should be an identity of the
  // original.
  rewriter_->RewriteEvent(data.incoming_event,
                          continuation.weak_ptr_factory_.GetWeakPtr());
  ASSERT_TRUE(continuation.passthrough_event);
  ASSERT_TRUE(continuation.passthrough_event->IsMouseEvent());
  EXPECT_EQ(ConvertToString(data.incoming_event),
            ConvertToString(*continuation.passthrough_event));
}

class GraphicsTabletButtonObserverTest
    : public PeripheralCustomizationEventRewriterTest,
      public testing::WithParamInterface<EventRewriterTestData> {};

INSTANTIATE_TEST_SUITE_P(
    All,
    GraphicsTabletButtonObserverTest,
    testing::ValuesIn(std::vector<EventRewriterTestData>{
        {
            CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                   ui::EF_RIGHT_MOUSE_BUTTON,
                                   ui::EF_RIGHT_MOUSE_BUTTON),
            absl::nullopt,
            mojom::CustomizableButton::kRight,
        },
        {
            CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                   ui::EF_BACK_MOUSE_BUTTON,
                                   ui::EF_BACK_MOUSE_BUTTON),
            absl::nullopt,
            mojom::CustomizableButton::kBack,
        },
        {
            CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                   ui::EF_FORWARD_MOUSE_BUTTON,
                                   ui::EF_FORWARD_MOUSE_BUTTON),
            absl::nullopt,
            mojom::CustomizableButton::kForward,
        },
        {
            CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                   ui::EF_MIDDLE_MOUSE_BUTTON,
                                   ui::EF_MIDDLE_MOUSE_BUTTON),
            absl::nullopt,
            mojom::CustomizableButton::kMiddle,
        },
        {
            CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                   ui::EF_MIDDLE_MOUSE_BUTTON |
                                       ui::EF_LEFT_MOUSE_BUTTON,
                                   ui::EF_MIDDLE_MOUSE_BUTTON),
            absl::nullopt,
            mojom::CustomizableButton::kMiddle,
        },

        // Observer notified only when the button is pressed.
        {
            CreateMouseButtonEvent(ui::ET_MOUSE_RELEASED,
                                   ui::EF_BACK_MOUSE_BUTTON,
                                   ui::EF_BACK_MOUSE_BUTTON),
            absl::nullopt,
        },

        // Left click ignored for buttons from a graphics tablet.
        {
            CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                   ui::EF_LEFT_MOUSE_BUTTON,
                                   ui::EF_LEFT_MOUSE_BUTTON),
            CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                   ui::EF_LEFT_MOUSE_BUTTON,
                                   ui::EF_LEFT_MOUSE_BUTTON),
        },

        // Other flags are ignored when included in the event with other
        // buttons.
        {
            CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                   ui::EF_LEFT_MOUSE_BUTTON |
                                       ui::EF_BACK_MOUSE_BUTTON,
                                   ui::EF_LEFT_MOUSE_BUTTON),
            CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                   ui::EF_LEFT_MOUSE_BUTTON,
                                   ui::EF_LEFT_MOUSE_BUTTON),
        },
        {
            CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                   ui::EF_LEFT_MOUSE_BUTTON |
                                       ui::EF_MIDDLE_MOUSE_BUTTON,
                                   ui::EF_NONE),
            CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                   ui::EF_LEFT_MOUSE_BUTTON,
                                   ui::EF_NONE),
        },
    }),
    [](const testing::TestParamInfo<EventRewriterTestData>& info) {
      std::string name = ConvertToString(info.param.incoming_event);
      std::replace(name.begin(), name.end(), ' ', '_');
      std::replace(name.begin(), name.end(), '=', '_');
      return name;
    });

TEST_P(GraphicsTabletButtonObserverTest, RewriteEvent) {
  const auto& data = GetParam();

  rewriter_->StartObservingGraphicsTablet(kDeviceId);

  TestEventRewriterContinuation continuation;
  rewriter_->RewriteEvent(data.incoming_event,
                          continuation.weak_ptr_factory_.GetWeakPtr());
  if (!data.rewritten_event) {
    ASSERT_TRUE(continuation.discarded());
    if (data.pressed_button) {
      const auto& actual_pressed_buttons =
          observer_->pressed_graphics_tablet_buttons().at(kDeviceId);
      ASSERT_EQ(1u, actual_pressed_buttons.size());
      EXPECT_EQ(*data.pressed_button, *actual_pressed_buttons[0]);
    }
  } else {
    ASSERT_TRUE(continuation.passthrough_event);
    ASSERT_TRUE(continuation.passthrough_event->IsMouseEvent());
    EXPECT_EQ(ConvertToString(*data.rewritten_event),
              ConvertToString(*continuation.passthrough_event));
  }

  rewriter_->StopObserving();
  continuation.reset();

  // After we stop observing, the passthrough event should be an identity of the
  // original.
  rewriter_->RewriteEvent(data.incoming_event,
                          continuation.weak_ptr_factory_.GetWeakPtr());
  ASSERT_TRUE(continuation.passthrough_event);
  ASSERT_TRUE(continuation.passthrough_event->IsMouseEvent());
  EXPECT_EQ(ConvertToString(data.incoming_event),
            ConvertToString(*continuation.passthrough_event));
}

}  // namespace ash
