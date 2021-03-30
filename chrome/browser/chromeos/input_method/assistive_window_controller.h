// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_ASSISTIVE_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_ASSISTIVE_WINDOW_CONTROLLER_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/chromeos/input_method/assistive_window_properties.h"
#include "chrome/browser/chromeos/input_method/tts_handler.h"
#include "chrome/browser/chromeos/input_method/ui/assistive_delegate.h"
#include "chrome/browser/chromeos/input_method/ui/suggestion_window_view.h"
#include "chrome/browser/chromeos/input_method/ui/undo_window.h"
#include "ui/base/ime/chromeos/ime_assistive_window_handler_interface.h"
#include "ui/gfx/native_widget_types.h"

class Profile;

namespace views {
class Widget;
}  // namespace views

namespace chromeos {

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
      std::unique_ptr<TtsHandler> tts_handler = nullptr);
  ~AssistiveWindowController() override;

  ui::ime::SuggestionWindowView* GetSuggestionWindowViewForTesting();
  ui::ime::UndoWindow* GetUndoWindowForTesting() const;

 private:
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
  void OnWidgetClosing(views::Widget* widget) override;

  // ui::ime::AssistiveDelegate implementation.
  void AssistiveWindowButtonClicked(
      const ui::ime::AssistiveWindowButton& button) const override;

  void InitSuggestionWindow();
  void InitUndoWindow();

  const AssistiveWindowControllerDelegate* delegate_;
  // The handler to handle Text-to-Speech (TTS) request.
  std::unique_ptr<TtsHandler> const tts_handler_;
  AssistiveWindowProperties window_;
  ui::ime::SuggestionWindowView* suggestion_window_view_ = nullptr;
  ui::ime::UndoWindow* undo_window_ = nullptr;
  std::u16string suggestion_text_;
  size_t confirmed_length_ = 0;
  Bounds bounds_;

  DISALLOW_COPY_AND_ASSIGN(AssistiveWindowController);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_ASSISTIVE_WINDOW_CONTROLLER_H_
