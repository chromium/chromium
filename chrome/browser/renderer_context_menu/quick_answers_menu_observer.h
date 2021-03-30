// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_CONTEXT_MENU_QUICK_ANSWERS_MENU_OBSERVER_H_
#define CHROME_BROWSER_RENDERER_CONTEXT_MENU_QUICK_ANSWERS_MENU_OBSERVER_H_

#include <memory>
#include <string>

#include "base/time/time.h"
#include "chromeos/components/quick_answers/quick_answers_client.h"
#include "components/renderer_context_menu/render_view_context_menu_observer.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {
class QuickAnswersController;
}

class RenderViewContextMenuProxy;

// A class that implements the quick answers menu.
class QuickAnswersMenuObserver
    : public RenderViewContextMenuObserver,
      public chromeos::quick_answers::QuickAnswersDelegate {
 public:
  QuickAnswersMenuObserver(const QuickAnswersMenuObserver&) = delete;
  QuickAnswersMenuObserver& operator=(const QuickAnswersMenuObserver&) = delete;

  explicit QuickAnswersMenuObserver(RenderViewContextMenuProxy* proxy);
  ~QuickAnswersMenuObserver() override;

  // RenderViewContextMenuObserver implementation.
  void CommandWillBeExecuted(int command_id) override;
  void OnContextMenuShown(const content::ContextMenuParams& params,
                          const gfx::Rect& bounds_in_screen) override;
  void OnContextMenuViewBoundsChanged(
      const gfx::Rect& bounds_in_screen) override;
  void OnMenuClosed() override;

  // QuickAnswersDelegate implementation.
  void OnQuickAnswerReceived(
      std::unique_ptr<chromeos::quick_answers::QuickAnswer> answer) override {}
  void OnEligibilityChanged(bool eligible) override;
  void OnNetworkError() override {}

  void SetQuickAnswerControllerForTesting(
      ash::QuickAnswersController* controller) {
    quick_answers_controller_ = controller;
  }

 private:
  std::string GetDeviceLanguage();
  void OnTextSurroundingSelectionAvailable(
      const std::string& selected_text,
      const std::u16string& surrounding_text,
      uint32_t start_offset,
      uint32_t end_offset);

  // The interface to add a context-menu item and update it.
  RenderViewContextMenuProxy* proxy_;

  std::unique_ptr<chromeos::quick_answers::QuickAnswersClient>
      quick_answers_client_;

  // Whether the feature is enabled and all eligibility criteria are met (
  // locale, consents, etc).
  bool is_eligible_ = false;

  gfx::Rect bounds_in_screen_;

  ash::QuickAnswersController* quick_answers_controller_ = nullptr;

  // Whether commands other than quick answers is executed.
  bool is_other_command_executed_ = false;

  // Time that the context menu is shown.
  base::TimeTicks menu_shown_time_;

  base::WeakPtrFactory<QuickAnswersMenuObserver> weak_factory_{this};
};

#endif  // CHROME_BROWSER_RENDERER_CONTEXT_MENU_QUICK_ANSWERS_MENU_OBSERVER_H_
