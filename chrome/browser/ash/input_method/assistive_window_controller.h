// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_ASSISTIVE_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_ASSISTIVE_WINDOW_CONTROLLER_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/input_method/assistive_window_properties.h"
#include "chrome/browser/ui/ash/input_method/announcement_view.h"
#include "chrome/browser/ui/ash/input_method/assistive_delegate.h"
#include "chrome/browser/ui/ash/input_method/grammar_suggestion_window.h"
#include "chrome/browser/ui/ash/input_method/suggestion_window_view.h"
#include "chrome/browser/ui/ash/input_method/undo_window.h"
#include "chromeos/ash/services/ime/public/cpp/assistive_suggestions.h"
#include "ui/base/ime/ash/ime_assistive_window_handler_interface.h"
#include "ui/gfx/native_widget_types.h"

class Profile;

namespace views {
class Widget;
}  // namespace views

namespace ash {
namespace input_method {

class AssistiveWindowControllerDelegate;

// AssistiveWindowController controls different assistive windows.
class AssistiveWindowController : public views::WidgetObserver,
                                  public IMEAssistiveWindowHandlerInterface,
                                  public ui::ime::AssistiveDelegate {
 public:
  explicit AssistiveWindowController(
      AssistiveWindowControllerDelegate* delegate,
      Profile* profile,
      ui::ime::AnnouncementView* announcement_view = nullptr);

  AssistiveWindowController(const AssistiveWindowController&) = delete;
  AssistiveWindowController& operator=(const AssistiveWindowController&) =
      delete;

  ~AssistiveWindowController() override;

  ui::ime::SuggestionWindowView* GetSuggestionWindowViewForTesting();
  ui::ime::UndoWindow* GetUndoWindowForTesting() const;

  // IMEAssistiveWindowHandlerInterface implementation.
  void SetBounds(const Bounds& bounds) override;
  void SetAssistiveWindowProperties(
      const AssistiveWindowProperties& window) override;
  void ShowSuggestion(const ui::ime::SuggestionDetails& details) override;
  void SetButtonHighlighted(const ui::ime::AssistiveWindowButton& button,
                            bool highlighted) override;
  void AcceptSuggestion(const std::u16string& suggestion) override;
  void HideSuggestion() override;
  std::u16string GetSuggestionText() const override;
  size_t GetConfirmedLength() const override;
  void FocusStateChanged() override;
  void OnWidgetDestroying(views::Widget* widget) override;
  void Announce(const std::u16string& message) override;

  // ui::ime::AssistiveDelegate implementation.
  void AssistiveWindowButtonClicked(
      const ui::ime::AssistiveWindowButton& button) const override;
  void AssistiveWindowChanged(
      const ash::ime::AssistiveWindow& window) const override;

 private:
  ui::ime::SuggestionWindowView::Orientation WindowOrientationFor(
      ash::ime::AssistiveWindowType window_type);
  void InitSuggestionWindow(
      ui::ime::SuggestionWindowView::Orientation orientation);
  void InitUndoWindow();
  void InitGrammarSuggestionWindow();
  void InitAnnouncementView();
  void DisplayCompletionSuggestion(const ui::ime::SuggestionDetails& details);
  void ClearPendingSuggestionTimer();

  raw_ptr<const AssistiveWindowControllerDelegate> delegate_;
  AssistiveWindowProperties window_;
  raw_ptr<ui::ime::SuggestionWindowView> suggestion_window_view_ = nullptr;
  raw_ptr<ui::ime::UndoWindow> undo_window_ = nullptr;
  raw_ptr<ui::ime::GrammarSuggestionWindow> grammar_suggestion_window_ =
      nullptr;
  raw_ptr<ui::ime::AnnouncementView> announcement_view_ = nullptr;
  std::u16string suggestion_text_;
  size_t confirmed_length_ = 0;
  Bounds bounds_;
  std::unique_ptr<base::OneShotTimer> pending_suggestion_timer_;

  base::WeakPtrFactory<AssistiveWindowController> weak_ptr_factory_{this};
};

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_ASSISTIVE_WINDOW_CONTROLLER_H_
