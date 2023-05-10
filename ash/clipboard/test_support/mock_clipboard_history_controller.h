// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_TEST_SUPPORT_MOCK_CLIPBOARD_HISTORY_CONTROLLER_H_
#define ASH_CLIPBOARD_TEST_SUPPORT_MOCK_CLIPBOARD_HISTORY_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/clipboard_history_controller.h"
#include "ash/public/cpp/scoped_clipboard_history_pause.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace crosapi::mojom {
enum class ClipboardHistoryControllerShowSource;
}  // namespace crosapi::mojom

namespace gfx {
class Rect;
}  // namespace gfx

namespace ui {
enum MenuSourceType;
}  // namespace ui

namespace ash {

class MockClipboardHistoryController : public ClipboardHistoryController {
 public:
  MockClipboardHistoryController();
  MockClipboardHistoryController(const MockClipboardHistoryController&) =
      delete;
  MockClipboardHistoryController& operator=(
      const MockClipboardHistoryController&) = delete;
  ~MockClipboardHistoryController() override;

  MOCK_METHOD(void, AddObserver, (Observer*), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer*), (override));
  MOCK_METHOD(bool, HasAvailableHistoryItems, (), (const, override));
  MOCK_METHOD(bool,
              ShowMenu,
              (const gfx::Rect&,
               ui::MenuSourceType,
               crosapi::mojom::ClipboardHistoryControllerShowSource),
              (override));
  MOCK_METHOD(bool,
              ShowMenu,
              (const gfx::Rect&,
               ui::MenuSourceType,
               crosapi::mojom::ClipboardHistoryControllerShowSource,
               OnMenuClosingCallback),
              (override));
  MOCK_METHOD(void, OnScreenshotNotificationCreated, (), (override));
  MOCK_METHOD(std::unique_ptr<ScopedClipboardHistoryPause>,
              CreateScopedPause,
              (),
              (override));
  MOCK_METHOD(void,
              GetHistoryValues,
              (GetHistoryValuesCallback),
              (const, override));
  MOCK_METHOD(std::vector<std::string>,
              GetHistoryItemIds,
              (),
              (const, override));
  MOCK_METHOD(bool,
              PasteClipboardItemById,
              (const std::string&,
               int,
               crosapi::mojom::ClipboardHistoryControllerShowSource),
              (override));
  MOCK_METHOD(bool, DeleteClipboardItemById, (const std::string&), (override));
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_TEST_SUPPORT_MOCK_CLIPBOARD_HISTORY_CONTROLLER_H_
