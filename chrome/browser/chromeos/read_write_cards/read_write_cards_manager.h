// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_READ_WRITE_CARDS_READ_WRITE_CARDS_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_READ_WRITE_CARDS_READ_WRITE_CARDS_MANAGER_H_

#include <memory>

#include "components/keyed_service/core/keyed_service.h"

class QuickAnswersControllerImpl;

namespace content {
struct ContextMenuParams;
}  // namespace content

namespace chromeos {

namespace editor_menu {
class EditorMenuControllerImpl;
}  // namespace editor_menu

class ReadWriteCardController;

// `ReadWriteCardsManager` provides supported UI controller to given context
// menu params. It could be either QuickAnswersController or
// EditorMenuController.
class ReadWriteCardsManager : public KeyedService {
 public:
  ReadWriteCardsManager();
  ReadWriteCardsManager(const ReadWriteCardsManager&) = delete;
  ReadWriteCardsManager& operator=(const ReadWriteCardsManager&) = delete;
  ~ReadWriteCardsManager() override;

  // KeyedService:
  void Shutdown() override;

  // Returns the supported controller for the input params. Could be nullptr if
  // it is not supported.
  ReadWriteCardController* GetController(
      const content::ContextMenuParams& params);

  chromeos::editor_menu::EditorMenuControllerImpl* editor_menu_for_testing() {
    return editor_menu_controller_.get();
  }

 private:
  std::unique_ptr<QuickAnswersControllerImpl> quick_answers_controller_;
  std::unique_ptr<chromeos::editor_menu::EditorMenuControllerImpl>
      editor_menu_controller_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_READ_WRITE_CARDS_READ_WRITE_CARDS_MANAGER_H_
