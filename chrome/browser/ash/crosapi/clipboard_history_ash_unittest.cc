// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/clipboard_history_ash.h"

#include <string>

#include "ash/clipboard/test_support/mock_clipboard_history_controller.h"
#include "base/unguessable_token.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_constants.h"

namespace crosapi {

class ClipboardHistoryAshTest : public testing::Test {
 public:
  ClipboardHistoryAsh clipboard_history_ash_;
  ash::MockClipboardHistoryController clipboard_history_controller_;
};

// Verifies that `ClipboardHistoryAsh` uses `ash::ClipboardHistoryController` to
// paste clipboard items by ID.
TEST_F(ClipboardHistoryAshTest, PasteClipboardItemById) {
  struct {
    base::UnguessableToken item_id;
    int event_flags;
    crosapi::mojom::ClipboardHistoryControllerShowSource paste_source;
  } test_params[] = {{base::UnguessableToken::Create(), ui::EF_NONE,
                      crosapi::mojom::ClipboardHistoryControllerShowSource::
                          kRenderViewContextMenu},
                     {base::UnguessableToken::Create(), ui::EF_MOUSE_BUTTON,
                      crosapi::mojom::ClipboardHistoryControllerShowSource::
                          kTextfieldContextMenu}};

  for (const auto& [id, event_flags, paste_source] : test_params) {
    EXPECT_CALL(
        clipboard_history_controller_,
        PasteClipboardItemById(id.ToString(), event_flags, paste_source));
    clipboard_history_ash_.PasteClipboardItemById(id, event_flags,
                                                  paste_source);
  }
}

}  // namespace crosapi
