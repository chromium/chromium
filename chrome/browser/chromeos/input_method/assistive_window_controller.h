// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_ASSISTIVE_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_ASSISTIVE_WINDOW_CONTROLLER_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/chromeos/input_method/ui/assistive_delegate.h"
#include "chrome/browser/chromeos/input_method/ui/suggestion_window_view.h"
#include "chrome/browser/chromeos/input_method/ui/undo_window.h"
#include "ui/base/ime/ime_assistive_window_handler_interface.h"
#include "ui/gfx/native_widget_types.h"

namespace views {
class Widget;
}  // namespace views

namespace chromeos {
struct AssistiveWindowProperties;

namespace input_method {

class AssistiveWindowControllerDelegate;

// AssistiveWindowController controls different assistive windows.
class AssistiveWindowController : public views::WidgetObserver,
                                  public IMEAssistiveWindowHandlerInterface,
                                  public ui::ime::AssistiveDelegate {
 public:
  explicit AssistiveWindowController(
      AssistiveWindowControllerDelegate* delegate);
  ~AssistiveWindowController() override;

  ui::ime::SuggestionWindowView* GetSuggestionWindowViewForTesting();
  ui::ime::UndoWindow* GetUndoWindowForTesting() const;

 private:
  // IMEAssistiveWindowHandlerInterface implementation.
  void SetBounds(const gfx::Rect& cursor_bounds) override;
  void SetAssistiveWindowProperties(
      const AssistiveWindowProperties& window) override;
  void ShowSuggestion(const base::string16& text,
                      const size_t confirmed_length,
                      const bool show_tab) override;
  void HideSuggestion() override;
  base::string16 GetSuggestionText() const override;
  size_t GetConfirmedLength() const override;
  void FocusStateChanged() override;
  void OnWidgetClosing(views::Widget* widget) override;

  // ui::ime::AssistiveDelegate implementation.
  void AssistiveWindowButtonClicked(
      ui::ime::ButtonId id,
      ui::ime::AssistiveWindowType type) const override;

  void InitSuggestionWindow();
  void InitUndoWindow();

  const AssistiveWindowControllerDelegate* delegate_;
  ui::ime::SuggestionWindowView* suggestion_window_view_ = nullptr;
  ui::ime::UndoWindow* undo_window_ = nullptr;
  base::string16 suggestion_text_;
  size_t confirmed_length_ = 0;

  DISALLOW_COPY_AND_ASSIGN(AssistiveWindowController);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_ASSISTIVE_WINDOW_CONTROLLER_H_
