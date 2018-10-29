// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_test_base.h"

#include "ash/shell.h"
#include "services/ws/test_window_tree_client.h"
#include "services/ws/window_tree_test_helper.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {

using CursorTest = AshTestBase;

TEST_F(CursorTest, TopLevel) {
  // Create a top level window.
  std::unique_ptr<aura::Window> window =
      CreateTestWindow(gfx::Rect(0, 0, 100, 100));

  // Make sure the WindowTreeClient receives events.
  EXPECT_EQ(0U, GetTestWindowTreeClient()->input_events().size());
  ui::test::EventGenerator generator(window.get());
  generator.MoveMouseToInHost(50, 50);
  ASSERT_EQ(1U, GetTestWindowTreeClient()->input_events().size());
  EXPECT_EQ(ui::EventType::ET_MOUSE_MOVED,
            GetTestWindowTreeClient()->PopInputEvent().event->type());

  // Check that WindowTree actually sets the cursor.
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(window->GetRootWindow());
  const ui::CursorData help_cursor(ui::CursorType::kHelp);
  GetWindowTreeTestHelper()->SetCursor(window.get(), help_cursor);
  EXPECT_EQ(ui::CursorType::kHelp,
            window->delegate()->GetCursor({}).native_type());
  EXPECT_EQ(ui::CursorType::kHelp, cursor_client->GetCursor().native_type());

  // If the mouse is not over the host, then SetCursor won't update the actual
  // cursor (i.e. the CursorClient).
  generator.MoveMouseToInHost(500, 500);
  const ui::CursorData not_allowed_cursor(ui::CursorType::kNotAllowed);
  GetWindowTreeTestHelper()->SetCursor(window.get(), not_allowed_cursor);
  EXPECT_EQ(ui::CursorType::kNotAllowed,
            window->delegate()->GetCursor({}).native_type());
  EXPECT_NE(ui::CursorType::kNotAllowed,
            cursor_client->GetCursor().native_type());
}

TEST_F(CursorTest, Embedded) {
  // Create a window to hold an embedding and set its cursor.
  aura::Window* embed_root = GetWindowTreeTestHelper()->NewWindow();
  ws::TestWindowTreeClient test_client;
  GetWindowTreeTestHelper()->Embed(embed_root, nullptr, &test_client, 0);
  const ui::CursorData help_cursor(ui::CursorType::kHelp);
  GetWindowTreeTestHelper()->SetCursor(embed_root, help_cursor);

  // Since the window isn't visible, the actual cursor shouldn't have changed.
  EXPECT_FALSE(embed_root->IsVisible());
  EXPECT_NE(ui::CursorType::kHelp,
            ash::Shell::Get()->cursor_manager()->GetCursor().native_type());

  // Create a top level window and put the embed root in it.
  std::unique_ptr<aura::Window> toplevel =
      CreateTestWindow(gfx::Rect(0, 0, 100, 100));
  toplevel->AddChild(embed_root);
  embed_root->SetBounds(toplevel->GetTargetBounds());
  embed_root->Show();
  EXPECT_TRUE(embed_root->IsVisible());

  // Now put the cursor over it and the previously set cursor should be used.
  ui::test::EventGenerator generator(toplevel.get());
  generator.MoveMouseToInHost(50, 50);
  EXPECT_EQ(ui::CursorType::kHelp,
            ash::Shell::Get()->cursor_manager()->GetCursor().native_type());

  // Setting to a new cursor should also immediately update the actual cursor.
  const ui::CursorData not_allowed_cursor(ui::CursorType::kNotAllowed);
  GetWindowTreeTestHelper()->SetCursor(embed_root, not_allowed_cursor);
  EXPECT_EQ(ui::CursorType::kNotAllowed,
            ash::Shell::Get()->cursor_manager()->GetCursor().native_type());
}

TEST_F(CursorTest, Custom) {
  // Create and hover a window.
  std::unique_ptr<aura::Window> window =
      CreateTestWindow(gfx::Rect(0, 0, 100, 100));
  EXPECT_EQ(0U, GetTestWindowTreeClient()->input_events().size());
  ui::test::EventGenerator generator(window.get());
  generator.MoveMouseToInHost(50, 50);

  // Set a custom cursor.
  SkBitmap bitmap = gfx::test::CreateBitmap(10, 10);
  const ui::CursorData image_cursor(gfx::Point(1, 4), {bitmap}, 1.f,
                                    base::TimeDelta());
  GetWindowTreeTestHelper()->SetCursor(window.get(), image_cursor);

  // Make sure it worked.
  EXPECT_EQ(ui::CursorType::kCustom,
            window->delegate()->GetCursor({}).native_type());
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(window->GetRootWindow());
  EXPECT_EQ(ui::CursorType::kCustom, cursor_client->GetCursor().native_type());
  EXPECT_EQ(bitmap.getGenerationID(),
            cursor_client->GetCursor().GetBitmap().getGenerationID());
}

}  // namespace ash
