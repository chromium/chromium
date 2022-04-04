// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/assistive_window_controller.h"

#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "chrome/browser/ash/input_method/assistive_window_controller_delegate.h"
#include "chrome/browser/ash/input_method/assistive_window_properties.h"
#include "chrome/browser/ash/input_method/ui/suggestion_details.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace input_method {

namespace {
gfx::NativeView GetParentView() {
  gfx::NativeView parent = nullptr;

  aura::Window* active_window = ash::window_util::GetActiveWindow();
  // Use MenuContainer so that it works even with a system modal dialog.
  parent = ash::Shell::GetContainer(
      active_window ? active_window->GetRootWindow()
                    : ash::Shell::GetRootWindowForNewWindows(),
      ash::kShellWindowId_MenuContainer);
  return parent;
}

bool IsLacrosEnabled() {
  return base::FeatureList::IsEnabled(chromeos::features::kLacrosSupport);
}

}  // namespace

AssistiveWindowController::AssistiveWindowController(
    AssistiveWindowControllerDelegate* delegate,
    Profile* profile,
    ui::ime::AssistiveAccessibilityView* accessibility_view)
    : delegate_(delegate), accessibility_view_(accessibility_view) {}

AssistiveWindowController::~AssistiveWindowController() {
  if (suggestion_window_view_ && suggestion_window_view_->GetWidget())
    suggestion_window_view_->GetWidget()->RemoveObserver(this);
  if (undo_window_ && undo_window_->GetWidget())
    undo_window_->GetWidget()->RemoveObserver(this);
  if (grammar_suggestion_window_ && grammar_suggestion_window_->GetWidget())
    grammar_suggestion_window_->GetWidget()->RemoveObserver(this);
  if (accessibility_view_ && accessibility_view_->GetWidget())
    accessibility_view_->GetWidget()->RemoveObserver(this);
  CHECK(!IsInObserverList());
}

void AssistiveWindowController::InitSuggestionWindow() {
  if (suggestion_window_view_)
    return;
  // suggestion_window_view_ is deleted by DialogDelegateView::DeleteDelegate.
  // TODO(b/215292569): Allow horizontal and vertical orientation to be toggled
  // via some parameter.
  suggestion_window_view_ = ui::ime::SuggestionWindowView::Create(
      GetParentView(), this,
      ui::ime::SuggestionWindowView::Orientation::kVertical);
  views::Widget* widget = suggestion_window_view_->GetWidget();
  widget->AddObserver(this);
  widget->Show();
}

void AssistiveWindowController::InitUndoWindow() {
  if (undo_window_)
    return;
  // undo_window is deleted by DialogDelegateView::DeleteDelegate.
  undo_window_ = new ui::ime::UndoWindow(GetParentView(), this);
  views::Widget* widget = undo_window_->InitWidget();
  widget->AddObserver(this);
  widget->Show();
}

void AssistiveWindowController::InitGrammarSuggestionWindow() {
  if (grammar_suggestion_window_)
    return;
  // grammar_suggestion_window_ is deleted by
  // DialogDelegateView::DeleteDelegate.
  grammar_suggestion_window_ =
      new ui::ime::GrammarSuggestionWindow(GetParentView(), this);
  views::Widget* widget = grammar_suggestion_window_->InitWidget();
  widget->AddObserver(this);
  widget->Show();
}

void AssistiveWindowController::InitAccessibilityView() {
  if (accessibility_view_)
    return;

  // accessibility_view_ is deleted by DialogDelegateView::DeleteDelegate.
  accessibility_view_ =
      new ui::ime::AssistiveAccessibilityView(GetParentView());
  accessibility_view_->GetWidget()->AddObserver(this);
}

void AssistiveWindowController::OnWidgetClosing(views::Widget* widget) {
  if (suggestion_window_view_ &&
      widget == suggestion_window_view_->GetWidget()) {
    widget->RemoveObserver(this);
    suggestion_window_view_ = nullptr;
  }
  if (undo_window_ && widget == undo_window_->GetWidget()) {
    widget->RemoveObserver(this);
    undo_window_ = nullptr;
  }
  if (grammar_suggestion_window_ &&
      widget == grammar_suggestion_window_->GetWidget()) {
    widget->RemoveObserver(this);
    grammar_suggestion_window_ = nullptr;
  }
  if (accessibility_view_ && widget == accessibility_view_->GetWidget()) {
    widget->RemoveObserver(this);
    accessibility_view_ = nullptr;
  }
}

void AssistiveWindowController::Announce(const std::u16string& message) {
  if (!accessibility_view_)
    InitAccessibilityView();

  accessibility_view_->Announce(message);
}

// TODO(crbug/1119570): Update AcceptSuggestion signature (either use
// announce_string, or no string)
void AssistiveWindowController::AcceptSuggestion(
    const std::u16string& suggestion) {
  if (window_.type == ui::ime::AssistiveWindowType::kEmojiSuggestion) {
    Announce(l10n_util::GetStringUTF16(IDS_SUGGESTION_EMOJI_SUGGESTED));
  } else {
    Announce(l10n_util::GetStringUTF16(IDS_SUGGESTION_INSERTED));
  }
  HideSuggestion();
}

void AssistiveWindowController::HideSuggestion() {
  suggestion_text_ = base::EmptyString16();
  confirmed_length_ = 0;
  tracking_last_suggestion_ = false;
  if (suggestion_window_view_)
    suggestion_window_view_->GetWidget()->Close();
  if (grammar_suggestion_window_)
    grammar_suggestion_window_->GetWidget()->Close();
}

void AssistiveWindowController::SetBounds(const Bounds& bounds) {
  bounds_ = bounds;
  // Sets suggestion_window_view_'s bounds here for most up-to-date cursor
  // position. This is different from UndoWindow because UndoWindow gets cursors
  // position before showing.
  // TODO(crbug/1112982): Investigate getting bounds to suggester before sending
  // show suggestion request.
  if (suggestion_window_view_ && !tracking_last_suggestion_) {
    // TODO(crbug/1146266): When running the multi word feature with lacros,
    //     composition mode is unavailable, thus we need to use the caret
    //     bounds instead. Investigate how we can position the window correctly
    //     without composition bounds.
    suggestion_window_view_->SetAnchorRect(
        (confirmed_length_ != 0 && !IsLacrosEnabled()) ? bounds.composition_text
                                                       : bounds.caret);
  }
  if (grammar_suggestion_window_) {
    grammar_suggestion_window_->SetBounds(bounds_.caret);
  }
}

void AssistiveWindowController::FocusStateChanged() {
  HideSuggestion();
  if (undo_window_)
    undo_window_->Hide();
}

void AssistiveWindowController::ShowSuggestion(
    const ui::ime::SuggestionDetails& details) {
  if (!suggestion_window_view_)
    InitSuggestionWindow();
  tracking_last_suggestion_ = suggestion_text_ == details.text;
  suggestion_text_ = details.text;
  confirmed_length_ = details.confirmed_length;
  suggestion_window_view_->Show(details);
}

void AssistiveWindowController::SetButtonHighlighted(
    const ui::ime::AssistiveWindowButton& button,
    bool highlighted) {
  switch (button.window_type) {
    case ui::ime::AssistiveWindowType::kEmojiSuggestion:
    case ui::ime::AssistiveWindowType::kPersonalInfoSuggestion:
    case ui::ime::AssistiveWindowType::kMultiWordSuggestion:
      if (!suggestion_window_view_)
        return;

      suggestion_window_view_->SetButtonHighlighted(button, highlighted);
      if (highlighted)
        Announce(button.announce_string);
      break;
    case ui::ime::AssistiveWindowType::kUndoWindow:
      if (!undo_window_)
        return;

      undo_window_->SetButtonHighlighted(button, highlighted);
      Announce(button.announce_string);
      break;
    case ui::ime::AssistiveWindowType::kGrammarSuggestion:
      if (!grammar_suggestion_window_)
        return;

      grammar_suggestion_window_->SetButtonHighlighted(button, highlighted);
      if (highlighted)
        Announce(button.announce_string);
      break;
    case ui::ime::AssistiveWindowType::kNone:
      break;
  }
}

std::u16string AssistiveWindowController::GetSuggestionText() const {
  return suggestion_text_;
}

size_t AssistiveWindowController::GetConfirmedLength() const {
  return confirmed_length_;
}

void AssistiveWindowController::SetAssistiveWindowProperties(
    const AssistiveWindowProperties& window) {
  window_ = window;
  switch (window.type) {
    case ui::ime::AssistiveWindowType::kUndoWindow:
      if (!undo_window_)
        InitUndoWindow();
      if (window.visible) {
        // Apply 4px padding to move the window away from the cursor.
        gfx::Rect anchor_rect =
            bounds_.autocorrect.IsEmpty() ? bounds_.caret : bounds_.autocorrect;
        anchor_rect.Inset(-4);
        undo_window_->SetAnchorRect(anchor_rect);
        undo_window_->Show();
      } else {
        undo_window_->Hide();
      }
      break;
    case ui::ime::AssistiveWindowType::kEmojiSuggestion:
    case ui::ime::AssistiveWindowType::kPersonalInfoSuggestion:
    case ui::ime::AssistiveWindowType::kMultiWordSuggestion:
      if (!suggestion_window_view_)
        InitSuggestionWindow();
      if (window_.visible) {
        suggestion_window_view_->ShowMultipleCandidates(window);
      } else {
        HideSuggestion();
      }
      break;
    case ui::ime::AssistiveWindowType::kGrammarSuggestion:
      if (window.candidates.size() == 0)
        return;
      if (!grammar_suggestion_window_)
        InitGrammarSuggestionWindow();
      if (window.visible) {
        grammar_suggestion_window_->SetBounds(bounds_.caret);
        grammar_suggestion_window_->SetSuggestion(window.candidates[0]);
        grammar_suggestion_window_->Show();
      } else {
        grammar_suggestion_window_->Hide();
      }
      break;
    case ui::ime::AssistiveWindowType::kNone:
      break;
  }
  Announce(window.announce_string);
}

void AssistiveWindowController::AssistiveWindowButtonClicked(
    const ui::ime::AssistiveWindowButton& button) const {
    delegate_->AssistiveWindowButtonClicked(button);
}

ui::ime::SuggestionWindowView*
AssistiveWindowController::GetSuggestionWindowViewForTesting() {
  return suggestion_window_view_;
}

ui::ime::UndoWindow* AssistiveWindowController::GetUndoWindowForTesting()
    const {
  return undo_window_;
}

}  // namespace input_method
}  // namespace ash
