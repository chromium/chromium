// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/accessibility/sticky_keys/sticky_keys_controller.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_source.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"

namespace ash {

class StickyKeysTest : public AshTestBase {
 public:
  StickyKeysTest(const StickyKeysTest&) = delete;
  StickyKeysTest& operator=(const StickyKeysTest&) = delete;

 protected:
  StickyKeysTest() = default;

  void SetUp() override {
    AshTestBase::SetUp();

    // |target_| owned by root window of shell. It is still safe to delete
    // it ourselves.
    target_ = CreateTestWindowInShellWithId(0);
    root_window_ = target_->GetRootWindow();
  }

  virtual void OnShortcutPressed() {
    if (target_) {
      delete target_;
      target_ = nullptr;
    }
  }

  ui::KeyEvent* GenerateKey(ui::EventType type, ui::KeyboardCode code) {
    return GenerateSynthesizedKeyEvent(type, code,
                                       ui::UsLayoutKeyboardCodeToDomCode(code));
  }

  // Creates a mouse event backed by a native XInput2 generic button event.
  // This is the standard native event on Chromebooks.
  ui::MouseEvent* GenerateMouseEvent(ui::EventType type) {
    return GenerateMouseEventAt(type, gfx::Point());
  }

  // Creates a mouse event backed by a native XInput2 generic button event.
  // The |location| should be in physical pixels.
  ui::MouseEvent* GenerateMouseEventAt(ui::EventType type,
                                       const gfx::Point& location) {
    return GenerateSynthesizedMouseEventAt(type, location);
  }

  ui::MouseWheelEvent* GenerateMouseWheelEvent(int wheel_delta) {
    return GenerateSynthesizedMouseWheelEvent(wheel_delta);
  }

  ui::ScrollEvent* GenerateScrollEvent(int scroll_delta) {
    ui::ScrollEvent* event =
        new ui::ScrollEvent(ui::EventType::kScroll, gfx::Point(0, 0),
                            ui::EventTimeForNow(), ui::EF_NONE,
                            0,             // x_offset
                            scroll_delta,  // y_offset
                            0,             // x_offset_ordinal
                            scroll_delta,  // y_offset_ordinal
                            2);            // finger_count
    ui::Event::DispatcherApi dispatcher(event);
    dispatcher.set_target(target_);
    return event;
  }

  ui::ScrollEvent* GenerateFlingScrollEvent(int fling_delta, bool is_cancel) {
    ui::ScrollEvent* event = new ui::ScrollEvent(
        is_cancel ? ui::EventType::kScrollFlingCancel
                  : ui::EventType::kScrollFlingStart,
        gfx::Point(0, 0), ui::EventTimeForNow(), ui::EF_NONE,
        0,            // x_velocity
        fling_delta,  // y_velocity
        0,            // x_velocity_ordinal
        fling_delta,  // y_velocity_ordinal
        11);          // finger_count
    ui::Event::DispatcherApi dispatcher(event);
    dispatcher.set_target(target_);
    return event;
  }

  // Creates a synthesized KeyEvent that is not backed by a native event.
  ui::KeyEvent* GenerateSynthesizedKeyEvent(ui::EventType type,
                                            ui::KeyboardCode key_code,
                                            ui::DomCode dom_code) {
    return new ui::KeyEvent(type, key_code, dom_code, ui::EF_NONE);
  }

  // Creates a synthesized MouseEvent that is not backed by a native event.
  ui::MouseEvent* GenerateSynthesizedMouseEventAt(ui::EventType event_type,
                                                  const gfx::Point& location) {
    ui::MouseEvent* event;
    if (event_type == ui::EventType::kMousewheel) {
      event = new ui::MouseWheelEvent(
          gfx::Vector2d(), location, location, ui::EventTimeForNow(),
          ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
    } else {
      event = new ui::MouseEvent(
          event_type, location, location, ui::EventTimeForNow(),
          ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
    }
    ui::Event::DispatcherApi dispatcher(event);
    dispatcher.set_target(target_);
    return event;
  }

  // Creates a synthesized mouse press or release event.
  ui::MouseEvent* GenerateSynthesizedMouseClickEvent(
      ui::EventType type,
      const gfx::Point& location) {
    return GenerateSynthesizedMouseEventAt(type, location);
  }

  // Creates a synthesized EventType::kMouseMoved event.
  ui::MouseEvent* GenerateSynthesizedMouseMoveEvent(
      const gfx::Point& location) {
    return GenerateSynthesizedMouseEventAt(ui::EventType::kMouseMoved,
                                           location);
  }

  // Creates a synthesized MouseWHeel event.
  ui::MouseWheelEvent* GenerateSynthesizedMouseWheelEvent(int wheel_delta) {
    std::unique_ptr<ui::MouseEvent> mev(GenerateSynthesizedMouseEventAt(
        ui::EventType::kMousewheel, gfx::Point(0, 0)));
    ui::MouseWheelEvent* event = new ui::MouseWheelEvent(*mev, 0, wheel_delta);
    ui::Event::DispatcherApi dispatcher(event);
    dispatcher.set_target(target_);
    return event;
  }

  void SendActivateStickyKeyPattern(StickyKeysHandler* handler,
                                    ui::KeyboardCode key_code) {
    bool released = false;
    int down_flags = 0;
    std::unique_ptr<ui::KeyEvent> ev;
    ev.reset(GenerateKey(ui::EventType::kKeyPressed, key_code));
    handler->HandleKeyEvent(*ev.get(), &down_flags, &released);
    ev.reset(GenerateKey(ui::EventType::kKeyReleased, key_code));
    handler->HandleKeyEvent(*ev.get(), &down_flags, &released);
  }

  void SendActivateStickyKeyPattern(StickyKeysHandler* handler,
                                    ui::KeyboardCode key_code,
                                    ui::DomCode dom_code) {
    bool released = false;
    int down_flags = 0;
    std::unique_ptr<ui::KeyEvent> ev;
    ev.reset(GenerateSynthesizedKeyEvent(ui::EventType::kKeyPressed, key_code,
                                         dom_code));
    handler->HandleKeyEvent(*ev.get(), &down_flags, &released);
    ev.reset(GenerateSynthesizedKeyEvent(ui::EventType::kKeyReleased, key_code,
                                         dom_code));
    handler->HandleKeyEvent(*ev.get(), &down_flags, &released);
  }

  bool HandleKeyEvent(const ui::KeyEvent& key_event,
                      StickyKeysHandler* handler,
                      int* down,
                      bool* up) {
    return handler->HandleKeyEvent(key_event, down, up);
  }

  int HandleKeyEventForDownFlags(const ui::KeyEvent& key_event,
                                 StickyKeysHandler* handler) {
    bool released = false;
    int down = 0;
    handler->HandleKeyEvent(key_event, &down, &released);
    return down;
  }

  aura::Window* target() { return target_; }

 private:
  // Owned by root window of shell, but we can still delete |target_| safely.
  raw_ptr<aura::Window, DanglingUntriaged> target_ = nullptr;
  // The root window of |target_|. Not owned.
  raw_ptr<aura::Window, DanglingUntriaged> root_window_ = nullptr;
};

TEST_F(StickyKeysTest, BasicOneshotScenarioTest) {
  std::unique_ptr<ui::KeyEvent> ev;
  StickyKeysHandler sticky_key(ui::EF_SHIFT_DOWN);

  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());

  // By typing Shift key, internal state become ENABLED.
  SendActivateStickyKeyPattern(&sticky_key, ui::VKEY_SHIFT);
  EXPECT_EQ(STICKY_KEY_STATE_ENABLED, sticky_key.current_state());

  ev.reset(GenerateKey(ui::EventType::kKeyPressed, ui::VKEY_A));
  bool released = false;
  int mod_down_flags = 0;
  HandleKeyEvent(*ev.get(), &sticky_key, &mod_down_flags, &released);
  // Next keyboard event is shift modified.
  EXPECT_TRUE(mod_down_flags & ui::EF_SHIFT_DOWN);
  // Modifier release notification happens.
  EXPECT_TRUE(released);

  ev.reset(GenerateKey(ui::EventType::kKeyReleased, ui::VKEY_A));
  released = false;
  mod_down_flags = 0;
  HandleKeyEvent(*ev.get(), &sticky_key, &mod_down_flags, &released);

  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());
  // Making sure Shift up keyboard event is available.
  std::unique_ptr<ui::Event> up_event;
  ASSERT_EQ(0, sticky_key.GetModifierUpEvent(&up_event));
  EXPECT_TRUE(up_event.get());
  EXPECT_EQ(ui::EventType::kKeyReleased, up_event->type());
  EXPECT_EQ(ui::VKEY_SHIFT,
            static_cast<const ui::KeyEvent*>(up_event.get())->key_code());

  // Enabled state is one shot, so next key event should not be shift modified.
  ev.reset(GenerateKey(ui::EventType::kKeyPressed, ui::VKEY_A));
  mod_down_flags = HandleKeyEventForDownFlags(*ev.get(), &sticky_key);
  EXPECT_FALSE(mod_down_flags & ui::EF_SHIFT_DOWN);

  ev.reset(GenerateKey(ui::EventType::kKeyReleased, ui::VKEY_A));
  mod_down_flags = HandleKeyEventForDownFlags(*ev.get(), &sticky_key);
  EXPECT_FALSE(mod_down_flags & ui::EF_SHIFT_DOWN);
}

TEST_F(StickyKeysTest, BasicOneshotScenarioFnTest) {
  std::unique_ptr<ui::KeyEvent> ev;
  StickyKeysHandler sticky_key(ui::EF_FUNCTION_DOWN);

  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());

  // By typing Fn key, internal state become ENABLED.
  SendActivateStickyKeyPattern(&sticky_key, ui::VKEY_FUNCTION);
  EXPECT_EQ(STICKY_KEY_STATE_ENABLED, sticky_key.current_state());

  ev.reset(GenerateKey(ui::EventType::kKeyPressed, ui::VKEY_A));
  bool released = false;
  int mod_down_flags = 0;
  HandleKeyEvent(*ev.get(), &sticky_key, &mod_down_flags, &released);
  // Next keyboard event is fn modified.
  EXPECT_TRUE(mod_down_flags & ui::EF_FUNCTION_DOWN);
  // Modifier release notification happens.
  EXPECT_TRUE(released);

  ev.reset(GenerateKey(ui::EventType::kKeyReleased, ui::VKEY_A));
  released = false;
  mod_down_flags = 0;
  HandleKeyEvent(*ev.get(), &sticky_key, &mod_down_flags, &released);

  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());
  // Making sure Function up keyboard event is available.
  std::unique_ptr<ui::Event> up_event;
  ASSERT_EQ(0, sticky_key.GetModifierUpEvent(&up_event));
  EXPECT_TRUE(up_event.get());
  EXPECT_EQ(ui::EventType::kKeyReleased, up_event->type());
  EXPECT_EQ(ui::VKEY_FUNCTION,
            static_cast<const ui::KeyEvent*>(up_event.get())->key_code());

  // Enabled state is one shot, so next key event should not be fn modified.
  ev.reset(GenerateKey(ui::EventType::kKeyPressed, ui::VKEY_A));
  mod_down_flags = HandleKeyEventForDownFlags(*ev.get(), &sticky_key);
  EXPECT_FALSE(mod_down_flags & ui::EF_FUNCTION_DOWN);

  ev.reset(GenerateKey(ui::EventType::kKeyReleased, ui::VKEY_A));
  mod_down_flags = HandleKeyEventForDownFlags(*ev.get(), &sticky_key);
  EXPECT_FALSE(mod_down_flags & ui::EF_FUNCTION_DOWN);
}

TEST_F(StickyKeysTest, AltGrKey) {
  std::unique_ptr<ui::KeyEvent> ev;
  StickyKeysHandler altgr_sticky_key(ui::EF_ALTGR_DOWN);
  StickyKeysHandler alt_sticky_key(ui::EF_ALT_DOWN);
  altgr_sticky_key.set_altgr_active(false);
  alt_sticky_key.set_altgr_active(false);

  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, altgr_sticky_key.current_state());
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, alt_sticky_key.current_state());

  // When the sticky key is not active, typing the right alt key doesn't trigger
  // the altgr sticky key handler.
  // On the internal keyboard, right alt has ui::VKEY_MENU and
  // ui::DomCode::ALT_RIGHT.
  SendActivateStickyKeyPattern(&altgr_sticky_key, ui::VKEY_MENU,
                               ui::DomCode::ALT_RIGHT);
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, altgr_sticky_key.current_state());

  SendActivateStickyKeyPattern(&alt_sticky_key, ui::VKEY_MENU,
                               ui::DomCode::ALT_RIGHT);
  EXPECT_EQ(STICKY_KEY_STATE_ENABLED, alt_sticky_key.current_state());

  // Key press is not modified by altgr, but is modified by alt.
  ev.reset(GenerateKey(ui::EventType::kKeyPressed, ui::VKEY_N));
  int mod_down_flags = 0;
  mod_down_flags = HandleKeyEventForDownFlags(*ev.get(), &altgr_sticky_key);
  EXPECT_FALSE(mod_down_flags & ui::EF_ALTGR_DOWN);
  mod_down_flags = HandleKeyEventForDownFlags(*ev.get(), &alt_sticky_key);
  EXPECT_TRUE(mod_down_flags & ui::EF_ALT_DOWN);

  ev.reset(GenerateKey(ui::EventType::kKeyReleased, ui::VKEY_N));
  mod_down_flags = HandleKeyEventForDownFlags(*ev.get(), &altgr_sticky_key);
  EXPECT_FALSE(mod_down_flags & ui::EF_ALTGR_DOWN);
  mod_down_flags = HandleKeyEventForDownFlags(*ev.get(), &alt_sticky_key);
  EXPECT_FALSE(mod_down_flags & ui::EF_ALT_DOWN);

  // Activate altgr for sticky keys.
  altgr_sticky_key.set_altgr_active(true);
  alt_sticky_key.set_altgr_active(true);

  // By typing altgr key with the key active, internal state becomes ENABLED for
  // altgr key.
  SendActivateStickyKeyPattern(&altgr_sticky_key, ui::VKEY_MENU,
                               ui::DomCode::ALT_RIGHT);
  EXPECT_EQ(STICKY_KEY_STATE_ENABLED, altgr_sticky_key.current_state());
  // Alt sticky key doesn't enable in this case.
  SendActivateStickyKeyPattern(&alt_sticky_key, ui::VKEY_MENU,
                               ui::DomCode::ALT_RIGHT);
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, alt_sticky_key.current_state());

  ev.reset(GenerateKey(ui::EventType::kKeyPressed, ui::VKEY_N));
  bool released = false;
  HandleKeyEvent(*ev.get(), &altgr_sticky_key, &mod_down_flags, &released);
  // Next keyboard event is altgr modified.
  EXPECT_TRUE(mod_down_flags & ui::EF_ALTGR_DOWN);
  // Modifier release notification happens.
  EXPECT_TRUE(released);

  // Next keyboard event is not alt modified.
  mod_down_flags = 0;
  HandleKeyEvent(*ev.get(), &altgr_sticky_key, &mod_down_flags, &released);
  EXPECT_FALSE(mod_down_flags & ui::EF_ALT_DOWN);

  ev.reset(GenerateKey(ui::EventType::kKeyReleased, ui::VKEY_N));
  released = false;
  mod_down_flags = 0;
  HandleKeyEvent(*ev.get(), &altgr_sticky_key, &mod_down_flags, &released);

  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, altgr_sticky_key.current_state());
  // Making sure altgr up keyboard event is available.
  std::unique_ptr<ui::Event> up_event;
  ASSERT_EQ(0, altgr_sticky_key.GetModifierUpEvent(&up_event));
  EXPECT_TRUE(up_event.get());
  EXPECT_EQ(ui::EventType::kKeyReleased, up_event->type());
  EXPECT_EQ(ui::VKEY_MENU,
            static_cast<const ui::KeyEvent*>(up_event.get())->key_code());
  EXPECT_EQ(ui::DomCode::ALT_RIGHT,
            static_cast<const ui::KeyEvent*>(up_event.get())->code());

  // Enabled state is one shot, so next key event should not be altgr modified.
  ev.reset(GenerateKey(ui::EventType::kKeyPressed, ui::VKEY_N));
  mod_down_flags = HandleKeyEventForDownFlags(*ev.get(), &altgr_sticky_key);
  EXPECT_FALSE(mod_down_flags & ui::EF_ALTGR_DOWN);

  ev.reset(GenerateKey(ui::EventType::kKeyReleased, ui::VKEY_N));
  mod_down_flags = HandleKeyEventForDownFlags(*ev.get(), &altgr_sticky_key);
  EXPECT_FALSE(mod_down_flags & ui::EF_ALTGR_DOWN);
}

TEST_F(StickyKeysTest, BasicLockedScenarioTest) {
  std::unique_ptr<ui::KeyEvent> ev;
  StickyKeysHandler sticky_key(ui::EF_SHIFT_DOWN);

  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());

  // By typing shift key, internal state become ENABLED.
  SendActivateStickyKeyPattern(&sticky_key, ui::VKEY_SHIFT);
  EXPECT_EQ(STICKY_KEY_STATE_ENABLED, sticky_key.current_state());

  // By typing shift key again, internal state become LOCKED.
  SendActivateStickyKeyPattern(&sticky_key, ui::VKEY_SHIFT);
  EXPECT_EQ(STICKY_KEY_STATE_LOCKED, sticky_key.current_state());

  // All keyboard events including keyUp become shift modified.
  ev.reset(GenerateKey(ui::EventType::kKeyPressed, ui::VKEY_A));
  int mod_down_flags = HandleKeyEventForDownFlags(*ev.get(), &sticky_key);
  EXPECT_TRUE(mod_down_flags & ui::EF_SHIFT_DOWN);

  ev.reset(GenerateKey(ui::EventType::kKeyReleased, ui::VKEY_A));
  mod_down_flags = HandleKeyEventForDownFlags(*ev.get(), &sticky_key);
  EXPECT_TRUE(mod_down_flags & ui::EF_SHIFT_DOWN);

  // Locked state keeps after normal keyboard event.
  EXPECT_EQ(STICKY_KEY_STATE_LOCKED, sticky_key.current_state());

  ev.reset(GenerateKey(ui::EventType::kKeyPressed, ui::VKEY_B));
  mod_down_flags = HandleKeyEventForDownFlags(*ev.get(), &sticky_key);
  EXPECT_TRUE(mod_down_flags & ui::EF_SHIFT_DOWN);

  ev.reset(GenerateKey(ui::EventType::kKeyReleased, ui::VKEY_B));
  mod_down_flags = HandleKeyEventForDownFlags(*ev.get(), &sticky_key);
  EXPECT_TRUE(mod_down_flags & ui::EF_SHIFT_DOWN);

  EXPECT_EQ(STICKY_KEY_STATE_LOCKED, sticky_key.current_state());

  // By typing shift key again, internal state become back to DISABLED.
  SendActivateStickyKeyPattern(&sticky_key, ui::VKEY_SHIFT);
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());
}

TEST_F(StickyKeysTest, NonTargetModifierTest) {
  std::unique_ptr<ui::KeyEvent> ev;
  StickyKeysHandler sticky_key(ui::EF_SHIFT_DOWN);

  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());

  // Non target modifier key does not affect internal state
  ev.reset(GenerateKey(ui::EventType::kKeyPressed, ui::VKEY_MENU));
  int mod_down_flags = HandleKeyEventForDownFlags(*ev.get(), &sticky_key);
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());
  EXPECT_EQ(ui::EF_NONE, mod_down_flags);

  ev.reset(GenerateKey(ui::EventType::kKeyReleased, ui::VKEY_MENU));
  mod_down_flags = HandleKeyEventForDownFlags(*ev.get(), &sticky_key);
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());
  EXPECT_EQ(ui::EF_NONE, mod_down_flags);

  SendActivateStickyKeyPattern(&sticky_key, ui::VKEY_SHIFT);
  EXPECT_EQ(STICKY_KEY_STATE_ENABLED, sticky_key.current_state());

  // Non target modifier key does not affect internal state
  ev.reset(GenerateKey(ui::EventType::kKeyPressed, ui::VKEY_MENU));
  mod_down_flags = HandleKeyEventForDownFlags(*ev.get(), &sticky_key);
  EXPECT_EQ(STICKY_KEY_STATE_ENABLED, sticky_key.current_state());
  EXPECT_EQ(ui::EF_NONE, mod_down_flags);

  ev.reset(GenerateKey(ui::EventType::kKeyReleased, ui::VKEY_MENU));
  mod_down_flags = HandleKeyEventForDownFlags(*ev.get(), &sticky_key);
  EXPECT_EQ(STICKY_KEY_STATE_ENABLED, sticky_key.current_state());
  EXPECT_EQ(ui::EF_NONE, mod_down_flags);

  SendActivateStickyKeyPattern(&sticky_key, ui::VKEY_SHIFT);
  EXPECT_EQ(STICKY_KEY_STATE_LOCKED, sticky_key.current_state());

  // Non target modifier key does not affect internal state
  ev.reset(GenerateKey(ui::EventType::kKeyPressed, ui::VKEY_MENU));
  mod_down_flags = HandleKeyEventForDownFlags(*ev.get(), &sticky_key);
  EXPECT_EQ(STICKY_KEY_STATE_LOCKED, sticky_key.current_state());
  EXPECT_EQ(ui::EF_NONE, mod_down_flags);

  ev.reset(GenerateKey(ui::EventType::kKeyReleased, ui::VKEY_MENU));
  mod_down_flags = HandleKeyEventForDownFlags(*ev.get(), &sticky_key);
  EXPECT_EQ(STICKY_KEY_STATE_LOCKED, sticky_key.current_state());
  EXPECT_EQ(ui::EF_NONE, mod_down_flags);
}

TEST_F(StickyKeysTest, NormalShortcutTest) {
  // Sticky keys should not be enabled if we perform a normal shortcut.
  std::unique_ptr<ui::KeyEvent> ev;
  StickyKeysHandler sticky_key(ui::EF_CONTROL_DOWN);

  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());

  // Perform ctrl+n shortcut.
  ev.reset(GenerateKey(ui::EventType::kKeyPressed, ui::VKEY_CONTROL));
  int mod_down_flags = HandleKeyEventForDownFlags(*ev.get(), &sticky_key);
  ev.reset(GenerateKey(ui::EventType::kKeyPressed, ui::VKEY_N));
  mod_down_flags = HandleKeyEventForDownFlags(*ev.get(), &sticky_key);
  ev.reset(GenerateKey(ui::EventType::kKeyReleased, ui::VKEY_N));
  mod_down_flags = HandleKeyEventForDownFlags(*ev.get(), &sticky_key);
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());
  ev.reset(GenerateKey(ui::EventType::kKeyReleased, ui::VKEY_CONTROL));
  mod_down_flags = HandleKeyEventForDownFlags(*ev.get(), &sticky_key);

  // Sticky keys should not be enabled afterwards.
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());
  EXPECT_EQ(ui::EF_NONE, mod_down_flags);

  // Perform ctrl+n shortcut, releasing ctrl first.
  ev.reset(GenerateKey(ui::EventType::kKeyPressed, ui::VKEY_CONTROL));
  mod_down_flags = HandleKeyEventForDownFlags(*ev.get(), &sticky_key);
  ev.reset(GenerateKey(ui::EventType::kKeyPressed, ui::VKEY_N));
  mod_down_flags = HandleKeyEventForDownFlags(*ev.get(), &sticky_key);
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());
  ev.reset(GenerateKey(ui::EventType::kKeyReleased, ui::VKEY_CONTROL));
  mod_down_flags = HandleKeyEventForDownFlags(*ev.get(), &sticky_key);
  ev.reset(GenerateKey(ui::EventType::kKeyReleased, ui::VKEY_N));
  mod_down_flags = HandleKeyEventForDownFlags(*ev.get(), &sticky_key);

  // Sticky keys should not be enabled afterwards.
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());
  EXPECT_EQ(ui::EF_NONE, mod_down_flags);
}

TEST_F(StickyKeysTest, NormalModifiedClickTest) {
  std::unique_ptr<ui::KeyEvent> kev;
  std::unique_ptr<ui::MouseEvent> mev;
  StickyKeysHandler sticky_key(ui::EF_CONTROL_DOWN);

  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());

  // Perform ctrl+click.
  kev.reset(GenerateKey(ui::EventType::kKeyPressed, ui::VKEY_CONTROL));
  int mod_down_flags = HandleKeyEventForDownFlags(*kev.get(), &sticky_key);
  mev.reset(GenerateMouseEvent(ui::EventType::kMousePressed));
  bool released = false;
  sticky_key.HandleMouseEvent(*mev.get(), &mod_down_flags, &released);
  mev.reset(GenerateMouseEvent(ui::EventType::kMouseReleased));
  sticky_key.HandleMouseEvent(*mev.get(), &mod_down_flags, &released);

  // Sticky keys should not be enabled afterwards.
  kev.reset(GenerateKey(ui::EventType::kKeyReleased, ui::VKEY_CONTROL));
  mod_down_flags = HandleKeyEventForDownFlags(*kev.get(), &sticky_key);
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());
  EXPECT_EQ(ui::EF_NONE, mod_down_flags);
}

TEST_F(StickyKeysTest, MouseMovedModifierTest) {
  std::unique_ptr<ui::KeyEvent> kev;
  std::unique_ptr<ui::MouseEvent> mev;
  StickyKeysHandler sticky_key(ui::EF_CONTROL_DOWN);

  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());

  // Press ctrl and handle mouse move events.
  kev.reset(GenerateKey(ui::EventType::kKeyPressed, ui::VKEY_CONTROL));
  int mod_down_flags = HandleKeyEventForDownFlags(*kev.get(), &sticky_key);
  mev.reset(GenerateSynthesizedMouseMoveEvent(gfx::Point(0, 0)));
  bool released = false;
  sticky_key.HandleMouseEvent(*mev.get(), &mod_down_flags, &released);
  mev.reset(GenerateSynthesizedMouseMoveEvent(gfx::Point(100, 100)));
  sticky_key.HandleMouseEvent(*mev.get(), &mod_down_flags, &released);

  // Sticky keys should be enabled afterwards.
  kev.reset(GenerateKey(ui::EventType::kKeyReleased, ui::VKEY_CONTROL));
  mod_down_flags = HandleKeyEventForDownFlags(*kev.get(), &sticky_key);
  EXPECT_EQ(STICKY_KEY_STATE_ENABLED, sticky_key.current_state());
  EXPECT_EQ(ui::EF_NONE, mod_down_flags);
}

TEST_F(StickyKeysTest, NormalModifiedScrollTest) {
  std::unique_ptr<ui::KeyEvent> kev;
  std::unique_ptr<ui::ScrollEvent> sev;
  StickyKeysHandler sticky_key(ui::EF_CONTROL_DOWN);

  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());

  // Perform ctrl+scroll.
  kev.reset(GenerateKey(ui::EventType::kKeyPressed, ui::VKEY_CONTROL));
  int mod_down_flags = HandleKeyEventForDownFlags(*kev.get(), &sticky_key);
  sev.reset(GenerateFlingScrollEvent(0, true));
  bool released = false;
  sticky_key.HandleScrollEvent(*sev.get(), &mod_down_flags, &released);
  sev.reset(GenerateScrollEvent(10));
  sticky_key.HandleScrollEvent(*sev.get(), &mod_down_flags, &released);
  sev.reset(GenerateFlingScrollEvent(10, false));
  sticky_key.HandleScrollEvent(*sev.get(), &mod_down_flags, &released);

  // Sticky keys should not be enabled afterwards.
  kev.reset(GenerateKey(ui::EventType::kKeyReleased, ui::VKEY_CONTROL));
  mod_down_flags = HandleKeyEventForDownFlags(*kev.get(), &sticky_key);
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());
  EXPECT_EQ(ui::EF_NONE, mod_down_flags);
}

TEST_F(StickyKeysTest, MouseEventOneshot) {
  std::unique_ptr<ui::MouseEvent> ev;
  std::unique_ptr<ui::KeyEvent> kev;
  StickyKeysHandler sticky_key(ui::EF_CONTROL_DOWN);

  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());
  SendActivateStickyKeyPattern(&sticky_key, ui::VKEY_CONTROL);
  EXPECT_EQ(STICKY_KEY_STATE_ENABLED, sticky_key.current_state());

  // We should still be in the ENABLED state until we get the mouse
  // release event.
  ev.reset(GenerateMouseEvent(ui::EventType::kMousePressed));
  bool released = false;
  int mod_down_flags = 0;
  sticky_key.HandleMouseEvent(*ev.get(), &mod_down_flags, &released);
  EXPECT_TRUE(mod_down_flags & ui::EF_CONTROL_DOWN);
  EXPECT_EQ(STICKY_KEY_STATE_ENABLED, sticky_key.current_state());

  ev.reset(GenerateMouseEvent(ui::EventType::kMouseReleased));
  released = false;
  mod_down_flags = 0;
  sticky_key.HandleMouseEvent(*ev.get(), &mod_down_flags, &released);
  EXPECT_TRUE(mod_down_flags & ui::EF_CONTROL_DOWN);
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());

  // Making sure modifier key release event is dispatched in the right order.
  EXPECT_TRUE(released);
  std::unique_ptr<ui::Event> up_event;
  ASSERT_EQ(0, sticky_key.GetModifierUpEvent(&up_event));
  EXPECT_TRUE(up_event.get());
  EXPECT_EQ(ui::EventType::kKeyReleased, up_event->type());
  EXPECT_EQ(ui::VKEY_CONTROL,
            static_cast<const ui::KeyEvent*>(up_event.get())->key_code());

  // Enabled state is one shot, so next click should not be control modified.
  ev.reset(GenerateMouseEvent(ui::EventType::kMousePressed));
  released = false;
  mod_down_flags = 0;
  sticky_key.HandleMouseEvent(*ev.get(), &mod_down_flags, &released);
  EXPECT_FALSE(mod_down_flags & ui::EF_CONTROL_DOWN);

  ev.reset(GenerateMouseEvent(ui::EventType::kMouseReleased));
  released = false;
  mod_down_flags = 0;
  sticky_key.HandleMouseEvent(*ev.get(), &mod_down_flags, &released);
  EXPECT_FALSE(mod_down_flags & ui::EF_CONTROL_DOWN);
}

TEST_F(StickyKeysTest, MouseEventLocked) {
  std::unique_ptr<ui::MouseEvent> ev;
  std::unique_ptr<ui::KeyEvent> kev;
  StickyKeysHandler sticky_key(ui::EF_CONTROL_DOWN);

  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());

  // Pressing modifier key twice should make us enter lock state.
  SendActivateStickyKeyPattern(&sticky_key, ui::VKEY_CONTROL);
  EXPECT_EQ(STICKY_KEY_STATE_ENABLED, sticky_key.current_state());
  SendActivateStickyKeyPattern(&sticky_key, ui::VKEY_CONTROL);
  EXPECT_EQ(STICKY_KEY_STATE_LOCKED, sticky_key.current_state());

  // Mouse events should not disable locked mode.
  for (int i = 0; i < 3; ++i) {
    bool released = false;
    int mod_down_flags = 0;
    ev.reset(GenerateMouseEvent(ui::EventType::kMousePressed));
    sticky_key.HandleMouseEvent(*ev.get(), &mod_down_flags, &released);
    EXPECT_TRUE(mod_down_flags & ui::EF_CONTROL_DOWN);
    ev.reset(GenerateMouseEvent(ui::EventType::kMouseReleased));
    released = false;
    mod_down_flags = 0;
    sticky_key.HandleMouseEvent(*ev.get(), &mod_down_flags, &released);
    EXPECT_TRUE(mod_down_flags & ui::EF_CONTROL_DOWN);
    EXPECT_EQ(STICKY_KEY_STATE_LOCKED, sticky_key.current_state());
  }

  // Test with mouse wheel.
  for (int i = 0; i < 3; ++i) {
    bool released = false;
    int mod_down_flags = 0;
    ev.reset(GenerateMouseWheelEvent(ui::MouseWheelEvent::kWheelDelta));
    sticky_key.HandleMouseEvent(*ev.get(), &mod_down_flags, &released);
    ev.reset(GenerateMouseWheelEvent(-ui::MouseWheelEvent::kWheelDelta));
    released = false;
    mod_down_flags = 0;
    sticky_key.HandleMouseEvent(*ev.get(), &mod_down_flags, &released);
    EXPECT_TRUE(mod_down_flags & ui::EF_CONTROL_DOWN);
    EXPECT_EQ(STICKY_KEY_STATE_LOCKED, sticky_key.current_state());
  }

  // Test mixed case with mouse events and key events.
  ev.reset(GenerateMouseWheelEvent(ui::MouseWheelEvent::kWheelDelta));
  bool released = false;
  int mod_down_flags = 0;
  sticky_key.HandleMouseEvent(*ev.get(), &mod_down_flags, &released);
  EXPECT_TRUE(mod_down_flags & ui::EF_CONTROL_DOWN);
  kev.reset(GenerateKey(ui::EventType::kKeyPressed, ui::VKEY_N));
  mod_down_flags = HandleKeyEventForDownFlags(*kev.get(), &sticky_key);
  EXPECT_TRUE(mod_down_flags & ui::EF_CONTROL_DOWN);
  mod_down_flags = HandleKeyEventForDownFlags(*kev.get(), &sticky_key);
  EXPECT_TRUE(mod_down_flags & ui::EF_CONTROL_DOWN);

  EXPECT_EQ(STICKY_KEY_STATE_LOCKED, sticky_key.current_state());
}

TEST_F(StickyKeysTest, ScrollEventOneshot) {
  std::unique_ptr<ui::ScrollEvent> ev;
  std::unique_ptr<ui::KeyEvent> kev;
  StickyKeysHandler sticky_key(ui::EF_CONTROL_DOWN);

  int scroll_deltas[] = {-10, 10};
  for (int i = 0; i < 2; ++i) {
    // Enable sticky keys.
    EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());
    SendActivateStickyKeyPattern(&sticky_key, ui::VKEY_CONTROL);
    EXPECT_EQ(STICKY_KEY_STATE_ENABLED, sticky_key.current_state());

    // Test a scroll sequence. Sticky keys should only be disabled at the end
    // of the scroll sequence. Fling cancel event starts the scroll sequence.
    ev.reset(GenerateFlingScrollEvent(0, true));
    bool released = false;
    int mod_down_flags = 0;
    sticky_key.HandleScrollEvent(*ev.get(), &mod_down_flags, &released);
    EXPECT_TRUE(mod_down_flags & ui::EF_CONTROL_DOWN);
    EXPECT_EQ(STICKY_KEY_STATE_ENABLED, sticky_key.current_state());

    // Scrolls should all be modified but not disable sticky keys.
    for (int j = 0; j < 3; ++j) {
      ev.reset(GenerateScrollEvent(scroll_deltas[i]));
      released = false;
      mod_down_flags = 0;
      sticky_key.HandleScrollEvent(*ev.get(), &mod_down_flags, &released);
      EXPECT_TRUE(mod_down_flags & ui::EF_CONTROL_DOWN);
      EXPECT_EQ(STICKY_KEY_STATE_ENABLED, sticky_key.current_state());
    }

    // Fling start event ends scroll sequence.
    ev.reset(GenerateFlingScrollEvent(scroll_deltas[i], false));
    released = false;
    mod_down_flags = 0;
    sticky_key.HandleScrollEvent(*ev.get(), &mod_down_flags, &released);
    EXPECT_TRUE(mod_down_flags & ui::EF_CONTROL_DOWN);
    EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());

    std::unique_ptr<ui::Event> up_event;
    EXPECT_TRUE(released);
    ASSERT_EQ(0, sticky_key.GetModifierUpEvent(&up_event));
    EXPECT_TRUE(up_event.get());
    EXPECT_EQ(ui::EventType::kKeyReleased, up_event->type());
    EXPECT_EQ(ui::VKEY_CONTROL,
              static_cast<const ui::KeyEvent*>(up_event.get())->key_code());
  }
}

TEST_F(StickyKeysTest, ScrollDirectionChanged) {
  std::unique_ptr<ui::ScrollEvent> ev;
  std::unique_ptr<ui::KeyEvent> kev;
  StickyKeysHandler sticky_key(ui::EF_CONTROL_DOWN);

  // Test direction change with both boundary value and negative value.
  const int direction_change_values[2] = {0, -10};
  for (int i = 0; i < 2; ++i) {
    SendActivateStickyKeyPattern(&sticky_key, ui::VKEY_CONTROL);
    EXPECT_EQ(STICKY_KEY_STATE_ENABLED, sticky_key.current_state());

    // Fling cancel starts scroll sequence.
    ev.reset(GenerateFlingScrollEvent(0, true));
    bool released = false;
    int mod_down_flags = 0;
    sticky_key.HandleScrollEvent(*ev.get(), &mod_down_flags, &released);
    EXPECT_EQ(STICKY_KEY_STATE_ENABLED, sticky_key.current_state());

    // Test that changing directions in a scroll sequence will
    // return sticky keys to DISABLED state.
    for (int j = 0; j < 3; ++j) {
      ev.reset(GenerateScrollEvent(10));
      released = false;
      mod_down_flags = 0;
      sticky_key.HandleScrollEvent(*ev.get(), &mod_down_flags, &released);
      EXPECT_TRUE(mod_down_flags & ui::EF_CONTROL_DOWN);
      EXPECT_EQ(STICKY_KEY_STATE_ENABLED, sticky_key.current_state());
    }

    ev.reset(GenerateScrollEvent(direction_change_values[i]));
    released = false;
    mod_down_flags = 0;
    sticky_key.HandleScrollEvent(*ev.get(), &mod_down_flags, &released);
    EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());
  }
}

TEST_F(StickyKeysTest, ScrollEventLocked) {
  std::unique_ptr<ui::ScrollEvent> ev;
  std::unique_ptr<ui::KeyEvent> kev;
  StickyKeysHandler sticky_key(ui::EF_CONTROL_DOWN);

  // Lock sticky keys.
  SendActivateStickyKeyPattern(&sticky_key, ui::VKEY_CONTROL);
  SendActivateStickyKeyPattern(&sticky_key, ui::VKEY_CONTROL);
  EXPECT_EQ(STICKY_KEY_STATE_LOCKED, sticky_key.current_state());

  // Test scroll events are correctly modified in locked state.
  for (int i = 0; i < 5; ++i) {
    // Fling cancel starts scroll sequence.
    ev.reset(GenerateFlingScrollEvent(0, true));
    bool released = false;
    int mod_down_flags = 0;
    sticky_key.HandleScrollEvent(*ev.get(), &mod_down_flags, &released);

    ev.reset(GenerateScrollEvent(10));
    released = false;
    mod_down_flags = 0;
    sticky_key.HandleScrollEvent(*ev.get(), &mod_down_flags, &released);
    EXPECT_TRUE(mod_down_flags & ui::EF_CONTROL_DOWN);
    ev.reset(GenerateScrollEvent(-10));
    sticky_key.HandleScrollEvent(*ev.get(), &mod_down_flags, &released);
    EXPECT_TRUE(mod_down_flags & ui::EF_CONTROL_DOWN);

    // Fling start ends scroll sequence.
    ev.reset(GenerateFlingScrollEvent(-10, false));
    sticky_key.HandleScrollEvent(*ev.get(), &mod_down_flags, &released);
  }

  EXPECT_EQ(STICKY_KEY_STATE_LOCKED, sticky_key.current_state());
}

TEST_F(StickyKeysTest, SynthesizedEvents) {
  // Non-native, internally generated events should be properly handled
  // by sticky keys.
  StickyKeysHandler sticky_key(ui::EF_CONTROL_DOWN);

  // Test non-native key events.
  std::unique_ptr<ui::KeyEvent> kev;
  SendActivateStickyKeyPattern(&sticky_key, ui::VKEY_CONTROL);
  EXPECT_EQ(STICKY_KEY_STATE_ENABLED, sticky_key.current_state());

  kev.reset(GenerateKey(ui::EventType::kKeyPressed, ui::VKEY_K));
  int mod_down_flags = HandleKeyEventForDownFlags(*kev.get(), &sticky_key);
  EXPECT_TRUE(mod_down_flags & ui::EF_CONTROL_DOWN);
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());

  kev.reset(GenerateKey(ui::EventType::kKeyReleased, ui::VKEY_K));
  mod_down_flags = HandleKeyEventForDownFlags(*kev.get(), &sticky_key);
  EXPECT_FALSE(mod_down_flags & ui::EF_CONTROL_DOWN);
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());

  // Test non-native mouse events.
  SendActivateStickyKeyPattern(&sticky_key, ui::VKEY_CONTROL);
  EXPECT_EQ(STICKY_KEY_STATE_ENABLED, sticky_key.current_state());

  std::unique_ptr<ui::MouseEvent> mev;
  mev.reset(GenerateSynthesizedMouseClickEvent(ui::EventType::kMousePressed,
                                               gfx::Point(0, 0)));
  bool released = false;
  sticky_key.HandleMouseEvent(*mev.get(), &mod_down_flags, &released);
  EXPECT_TRUE(mod_down_flags & ui::EF_CONTROL_DOWN);
  EXPECT_EQ(STICKY_KEY_STATE_ENABLED, sticky_key.current_state());

  mev.reset(GenerateSynthesizedMouseClickEvent(ui::EventType::kMouseReleased,
                                               gfx::Point(0, 0)));
  released = false;
  mod_down_flags = 0;
  sticky_key.HandleMouseEvent(*mev.get(), &mod_down_flags, &released);
  EXPECT_TRUE(mod_down_flags & ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(released);
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());
}

}  // namespace ash
