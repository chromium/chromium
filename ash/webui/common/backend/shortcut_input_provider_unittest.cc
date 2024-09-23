// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/common/backend/shortcut_input_provider.h"

#include <memory>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/webui/common/mojom/shortcut_input_provider.mojom.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/test/ash_test_suite.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/types/event_type.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

class FakeShortcutInputObserver : public common::mojom::ShortcutInputObserver {
 public:
  void OnShortcutInputEventPressed(
      ash::mojom::KeyEventPtr prerewritten_key_event,
      ash::mojom::KeyEventPtr key_event) override {
    ++num_input_events_pressed_;
  }
  void OnShortcutInputEventReleased(
      ash::mojom::KeyEventPtr prerewritten_key_event,
      ash::mojom::KeyEventPtr key_event) override {
    ++num_input_events_released_;
  }

  int num_input_events_pressed() { return num_input_events_pressed_; }
  int num_input_events_released() { return num_input_events_released_; }

  mojo::Receiver<common::mojom::ShortcutInputObserver> receiver{this};

 private:
  int num_input_events_pressed_ = 0;
  int num_input_events_released_ = 0;
};

ui::KeyEvent CreateKeyEvent(bool pressed) {
  return ui::KeyEvent(
      pressed ? ui::EventType::kKeyPressed : ui::EventType::kKeyReleased,
      ui::VKEY_A, ui::EF_NONE);
}

ui::KeyEvent CreateFnKeyEvent(bool pressed) {
  return ui::KeyEvent(
      pressed ? ui::EventType::kKeyPressed : ui::EventType::kKeyReleased,
      ui::VKEY_FUNCTION, ui::EF_NONE);
}

}  // namespace

class ShortcutInputProviderTest : public AshTestBase {
 public:
  ShortcutInputProviderTest() {
    scoped_feature_list_.InitWithFeatures({features::kPeripheralCustomization,
                                           features::kInputDeviceSettingsSplit},
                                          {});
  }

  void SetUp() override {
    ui::ResourceBundle::CleanupSharedInstance();
    AshTestSuite::LoadTestResources();
    AshTestBase::SetUp();

    shortcut_input_handler_ = Shell::Get()->shortcut_input_handler();
    shortcut_input_provider_ = std::make_unique<ShortcutInputProvider>();
    observer_ = std::make_unique<FakeShortcutInputObserver>();
    shortcut_input_provider_->StartObservingShortcutInput(
        observer_->receiver.BindNewPipeAndPassRemote());

    widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    widget_->Show();
    widget_->Activate();
  }

  void TearDown() override {
    shortcut_input_provider_.reset();
    shortcut_input_handler_ = nullptr;
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<ShortcutInputProvider> shortcut_input_provider_ = nullptr;
  raw_ptr<ShortcutInputHandler> shortcut_input_handler_;
  std::unique_ptr<FakeShortcutInputObserver> observer_;
  std::unique_ptr<views::Widget> widget_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ShortcutInputProviderTest, NoWidget) {
  // Prerewritten event needs to read first in order for the observer to fire.
  ui::KeyEvent prerewritten_pressed_event = CreateKeyEvent(/*pressed=*/true);
  ui::KeyEvent prerewritten_released_event = CreateKeyEvent(/*pressed=*/false);
  ui::KeyEvent pressed_event = CreateKeyEvent(/*pressed=*/true);
  ui::KeyEvent released_event = CreateKeyEvent(/*pressed=*/false);

  shortcut_input_handler_->OnPrerewriteKeyInputEvent(
      prerewritten_pressed_event);
  shortcut_input_handler_->OnKeyEvent(&pressed_event);
  shortcut_input_handler_->OnPrerewriteKeyInputEvent(
      prerewritten_released_event);
  shortcut_input_handler_->OnKeyEvent(&released_event);
  shortcut_input_provider_->FlushMojoForTesting();

  EXPECT_EQ(0, observer_->num_input_events_pressed());
  EXPECT_EQ(0, observer_->num_input_events_released());
  EXPECT_FALSE(Shell::Get()
                   ->accelerator_controller()
                   ->ShouldPreventProcessingAccelerators());
  EXPECT_FALSE(
      Shell::Get()->shortcut_input_handler()->should_consume_key_events());
}

TEST_F(ShortcutInputProviderTest, NoObservedEventWithoutprewrittenEvent) {
  ui::KeyEvent pressed_event = CreateKeyEvent(/*pressed=*/true);
  ui::KeyEvent released_event = CreateKeyEvent(/*pressed=*/false);
  shortcut_input_provider_->TieProviderToWidget(widget_.get());

  shortcut_input_handler_->OnKeyEvent(&pressed_event);
  shortcut_input_handler_->OnKeyEvent(&released_event);
  shortcut_input_provider_->FlushMojoForTesting();

  EXPECT_EQ(0, observer_->num_input_events_pressed());
  EXPECT_EQ(0, observer_->num_input_events_released());
  EXPECT_TRUE(Shell::Get()
                  ->accelerator_controller()
                  ->ShouldPreventProcessingAccelerators());
}

TEST_F(ShortcutInputProviderTest, PrewrittenEventOnlyDoesNotTriggerObserver) {
  ui::KeyEvent prerewritten_pressed_event = CreateKeyEvent(/*pressed=*/true);
  ui::KeyEvent prerewritten_released_event = CreateKeyEvent(/*pressed=*/false);
  shortcut_input_provider_->TieProviderToWidget(widget_.get());

  shortcut_input_handler_->OnPrerewriteKeyInputEvent(
      prerewritten_pressed_event);
  shortcut_input_handler_->OnPrerewriteKeyInputEvent(
      prerewritten_released_event);
  shortcut_input_provider_->FlushMojoForTesting();

  EXPECT_EQ(0, observer_->num_input_events_pressed());
  EXPECT_EQ(0, observer_->num_input_events_released());
  EXPECT_TRUE(Shell::Get()
                  ->accelerator_controller()
                  ->ShouldPreventProcessingAccelerators());
}

TEST_F(ShortcutInputProviderTest, SimpleEventFixFnInShortCutApp) {
  ui::KeyEvent prerewritten_pressed_event = CreateFnKeyEvent(/*pressed=*/true);
  ui::KeyEvent prerewritten_released_event =
      CreateFnKeyEvent(/*pressed=*/false);
  ui::KeyEvent pressed_event = CreateFnKeyEvent(/*pressed=*/true);
  ui::KeyEvent released_event = CreateFnKeyEvent(/*pressed=*/false);
  shortcut_input_provider_->TieProviderToWidget(widget_.get());

  shortcut_input_handler_->OnPrerewriteKeyInputEvent(
      prerewritten_pressed_event);
  shortcut_input_handler_->OnKeyEvent(&pressed_event);
  shortcut_input_handler_->OnPrerewriteKeyInputEvent(
      prerewritten_released_event);
  shortcut_input_handler_->OnKeyEvent(&released_event);
  shortcut_input_provider_->FlushMojoForTesting();

  EXPECT_EQ(1, observer_->num_input_events_pressed());
  EXPECT_EQ(1, observer_->num_input_events_released());
  EXPECT_TRUE(Shell::Get()
                  ->accelerator_controller()
                  ->ShouldPreventProcessingAccelerators());
  EXPECT_TRUE(
      Shell::Get()->shortcut_input_handler()->should_consume_key_events());
}

TEST_F(ShortcutInputProviderTest, SimpleEvent) {
  ui::KeyEvent prerewritten_pressed_event = CreateKeyEvent(/*pressed=*/true);
  ui::KeyEvent prerewritten_released_event = CreateKeyEvent(/*pressed=*/false);
  ui::KeyEvent pressed_event = CreateKeyEvent(/*pressed=*/true);
  ui::KeyEvent released_event = CreateKeyEvent(/*pressed=*/false);
  shortcut_input_provider_->TieProviderToWidget(widget_.get());

  shortcut_input_handler_->OnPrerewriteKeyInputEvent(
      prerewritten_pressed_event);
  shortcut_input_handler_->OnKeyEvent(&pressed_event);
  shortcut_input_handler_->OnPrerewriteKeyInputEvent(
      prerewritten_released_event);
  shortcut_input_handler_->OnKeyEvent(&released_event);
  shortcut_input_provider_->FlushMojoForTesting();

  EXPECT_EQ(1, observer_->num_input_events_pressed());
  EXPECT_EQ(1, observer_->num_input_events_released());
  EXPECT_TRUE(Shell::Get()
                  ->accelerator_controller()
                  ->ShouldPreventProcessingAccelerators());
  EXPECT_TRUE(
      Shell::Get()->shortcut_input_handler()->should_consume_key_events());
}

TEST_F(ShortcutInputProviderTest, SimpleEventNoFocus) {
  ui::KeyEvent prerewritten_pressed_event = CreateKeyEvent(/*pressed=*/true);
  ui::KeyEvent prerewritten_released_event = CreateKeyEvent(/*pressed=*/false);
  ui::KeyEvent pressed_event = CreateKeyEvent(/*pressed=*/true);
  ui::KeyEvent released_event = CreateKeyEvent(/*pressed=*/false);
  shortcut_input_provider_->TieProviderToWidget(widget_.get());

  widget_->Hide();

  shortcut_input_handler_->OnPrerewriteKeyInputEvent(
      prerewritten_pressed_event);
  shortcut_input_handler_->OnKeyEvent(&pressed_event);
  shortcut_input_handler_->OnPrerewriteKeyInputEvent(
      prerewritten_released_event);
  shortcut_input_handler_->OnKeyEvent(&released_event);
  shortcut_input_provider_->FlushMojoForTesting();

  EXPECT_EQ(0, observer_->num_input_events_pressed());
  EXPECT_EQ(0, observer_->num_input_events_released());
  EXPECT_FALSE(Shell::Get()
                   ->accelerator_controller()
                   ->ShouldPreventProcessingAccelerators());
  EXPECT_FALSE(
      Shell::Get()->shortcut_input_handler()->should_consume_key_events());

  widget_->Show();

  shortcut_input_handler_->OnPrerewriteKeyInputEvent(
      prerewritten_pressed_event);
  shortcut_input_handler_->OnKeyEvent(&pressed_event);
  shortcut_input_handler_->OnPrerewriteKeyInputEvent(
      prerewritten_released_event);
  shortcut_input_handler_->OnKeyEvent(&released_event);
  shortcut_input_provider_->FlushMojoForTesting();

  EXPECT_EQ(1, observer_->num_input_events_pressed());
  EXPECT_EQ(1, observer_->num_input_events_released());
  EXPECT_TRUE(Shell::Get()
                  ->accelerator_controller()
                  ->ShouldPreventProcessingAccelerators());
  EXPECT_TRUE(
      Shell::Get()->shortcut_input_handler()->should_consume_key_events());
}

TEST_F(ShortcutInputProviderTest, StopObservingTest) {
  ui::KeyEvent prerewritten_pressed_event = CreateKeyEvent(/*pressed=*/true);
  ui::KeyEvent prerewritten_released_event = CreateKeyEvent(/*pressed=*/false);
  ui::KeyEvent pressed_event = CreateKeyEvent(/*pressed=*/true);
  ui::KeyEvent released_event = CreateKeyEvent(/*pressed=*/false);
  shortcut_input_provider_->TieProviderToWidget(widget_.get());

  shortcut_input_handler_->OnPrerewriteKeyInputEvent(
      prerewritten_pressed_event);
  shortcut_input_handler_->OnKeyEvent(&pressed_event);
  shortcut_input_handler_->OnPrerewriteKeyInputEvent(
      prerewritten_released_event);
  shortcut_input_handler_->OnKeyEvent(&released_event);
  shortcut_input_provider_->FlushMojoForTesting();

  EXPECT_EQ(1, observer_->num_input_events_pressed());
  EXPECT_EQ(1, observer_->num_input_events_released());
  EXPECT_TRUE(Shell::Get()
                  ->accelerator_controller()
                  ->ShouldPreventProcessingAccelerators());
  EXPECT_TRUE(
      Shell::Get()->shortcut_input_handler()->should_consume_key_events());

  shortcut_input_provider_->StopObservingShortcutInput();
  shortcut_input_handler_->OnPrerewriteKeyInputEvent(
      prerewritten_pressed_event);
  shortcut_input_handler_->OnKeyEvent(&pressed_event);
  shortcut_input_handler_->OnPrerewriteKeyInputEvent(
      prerewritten_released_event);
  shortcut_input_handler_->OnKeyEvent(&released_event);
  shortcut_input_provider_->FlushMojoForTesting();

  EXPECT_EQ(1, observer_->num_input_events_pressed());
  EXPECT_EQ(1, observer_->num_input_events_released());
  EXPECT_FALSE(Shell::Get()
                   ->accelerator_controller()
                   ->ShouldPreventProcessingAccelerators());
  EXPECT_FALSE(
      Shell::Get()->shortcut_input_handler()->should_consume_key_events());
}

TEST_F(ShortcutInputProviderTest, WidgetDestroyedTest) {
  ui::KeyEvent prerewritten_pressed_event = CreateKeyEvent(/*pressed=*/true);
  ui::KeyEvent prerewritten_released_event = CreateKeyEvent(/*pressed=*/false);
  ui::KeyEvent pressed_event = CreateKeyEvent(/*pressed=*/true);
  ui::KeyEvent released_event = CreateKeyEvent(/*pressed=*/false);
  shortcut_input_provider_->TieProviderToWidget(widget_.get());

  shortcut_input_handler_->OnPrerewriteKeyInputEvent(
      prerewritten_pressed_event);
  shortcut_input_handler_->OnKeyEvent(&pressed_event);
  shortcut_input_handler_->OnPrerewriteKeyInputEvent(
      prerewritten_released_event);
  shortcut_input_handler_->OnKeyEvent(&released_event);
  shortcut_input_provider_->FlushMojoForTesting();

  EXPECT_EQ(1, observer_->num_input_events_pressed());
  EXPECT_EQ(1, observer_->num_input_events_released());

  // Destroy the widget, key events should not be communicated over mojo.
  widget_.reset();
  shortcut_input_handler_->OnPrerewriteKeyInputEvent(
      prerewritten_pressed_event);
  shortcut_input_handler_->OnKeyEvent(&pressed_event);
  shortcut_input_handler_->OnPrerewriteKeyInputEvent(
      prerewritten_released_event);
  shortcut_input_handler_->OnKeyEvent(&released_event);
  shortcut_input_provider_->FlushMojoForTesting();

  EXPECT_EQ(1, observer_->num_input_events_pressed());
  EXPECT_EQ(1, observer_->num_input_events_released());
}

}  // namespace ash
