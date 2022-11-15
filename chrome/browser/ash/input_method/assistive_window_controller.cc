// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/assistive_window_controller.h"

#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "base/strings/utf_string_conversions.h"
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

using ::ash::ime::AssistiveSuggestion;
using ::ash::ime::AssistiveSuggestionType;
using ::ash::ime::AssistiveWindow;
using ::ash::ime::AssistiveWindowType;

constexpr base::TimeDelta kShowSuggestionDelayMs = base::Milliseconds(5);

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

AssistiveWindowType ToAssistiveWindowType(
    const AssistiveSuggestionType& suggestion_type) {
  switch (suggestion_type) {
    case AssistiveSuggestionType::kAssistiveEmoji:
      return AssistiveWindowType::kEmojiSuggestion;
    case AssistiveSuggestionType::kAssistivePersonalInfo:
      return AssistiveWindowType::kPersonalInfoSuggestion;
    case AssistiveSuggestionType::kMultiWord:
      return AssistiveWindowType::kMultiWordSuggestion;
    case AssistiveSuggestionType::kGrammar:
      return AssistiveWindowType::kGrammarSuggestion;
    case AssistiveSuggestionType::kLongpressDiacritic:
      return AssistiveWindowType::kLongpressDiacriticsSuggestion;
    default:
      return AssistiveWindowType::kNone;
  }
}

AssistiveWindow SingleCandidateAssistiveWindow(
    const AssistiveWindowType& window_type,
    const AssistiveSuggestionType& suggestion_type,
    const std::u16string& candidate,
    size_t confirmed_length) {
  return AssistiveWindow(/*type=*/window_type,
                         /*candidates=*/std::vector<AssistiveSuggestion>{
                             AssistiveSuggestion{
                                 .type = suggestion_type,
                                 .text = base::UTF16ToUTF8(candidate),
                                 .confirmed_length = confirmed_length,
                             },
                         });
}

AssistiveWindow MultiCandidateAssistiveWindow(
    const AssistiveWindowType& window_type,
    const AssistiveSuggestionType& suggestion_type,
    const std::vector<std::u16string>& candidates) {
  std::vector<AssistiveSuggestion> suggestions;
  for (const auto& candidate : candidates) {
    suggestions.push_back(AssistiveSuggestion{
        .type = suggestion_type,
        .text = base::UTF16ToUTF8(candidate),
        .confirmed_length = 0,
    });
  }
  return AssistiveWindow(/*type=*/window_type,
                         /*candidates=*/suggestions);
}

AssistiveWindow UndoAssistiveWindow() {
  return AssistiveWindow(/*type=*/AssistiveWindowType::kUndoWindow,
                         /*suggestions=*/{});
}

AssistiveWindow HiddenAssistiveWindow() {
  return AssistiveWindow(/*type=*/AssistiveWindowType::kNone,
                         /*suggestions=*/{});
}

}  // namespace

AssistiveWindowController::AssistiveWindowController(
    AssistiveWindowControllerDelegate* delegate,
    Profile* profile,
    ui::ime::AssistiveAccessibilityView* accessibility_view)
    : delegate_(delegate), accessibility_view_(accessibility_view) {}

AssistiveWindowController::~AssistiveWindowController() {
  ClearPendingSuggestionTimer();
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

void AssistiveWindowController::InitSuggestionWindow(
    ui::ime::SuggestionWindowView::Orientation orientation) {
  if (suggestion_window_view_)
    return;
  // suggestion_window_view_ is deleted by DialogDelegateView::DeleteDelegate.
  suggestion_window_view_ =
      ui::ime::SuggestionWindowView::Create(GetParentView(), this, orientation);
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

void AssistiveWindowController::OnWidgetDestroying(views::Widget* widget) {
  ClearPendingSuggestionTimer();
  // Ensure we capture any AssistiveWindow state changes.
  if ((suggestion_window_view_ &&
       widget == suggestion_window_view_->GetWidget()) ||
      (undo_window_ && widget == undo_window_->GetWidget()) ||
      (grammar_suggestion_window_ &&
       widget == grammar_suggestion_window_->GetWidget())) {
    SetAssistiveWindow(HiddenAssistiveWindow());
  }
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
  if (window_.type == ash::ime::AssistiveWindowType::kEmojiSuggestion) {
    Announce(l10n_util::GetStringUTF16(IDS_SUGGESTION_EMOJI_SUGGESTED));
  } else {
    Announce(l10n_util::GetStringUTF16(IDS_SUGGESTION_INSERTED));
  }
  HideSuggestion();
}

void AssistiveWindowController::HideSuggestion() {
  suggestion_text_ = base::EmptyString16();
  confirmed_length_ = 0;
  if (suggestion_window_view_)
    suggestion_window_view_->GetWidget()->Close();
  if (grammar_suggestion_window_)
    grammar_suggestion_window_->GetWidget()->Close();
  SetAssistiveWindow(HiddenAssistiveWindow());
}

void AssistiveWindowController::SetBounds(const Bounds& bounds) {
  if (bounds == bounds_)
    return;

  bounds_ = bounds;
  if (suggestion_window_view_)
    suggestion_window_view_->SetAnchorRect(bounds.caret);
  if (grammar_suggestion_window_)
    grammar_suggestion_window_->SetBounds(bounds_.caret);
  if (pending_suggestion_timer_ && pending_suggestion_timer_->IsRunning()) {
    pending_suggestion_timer_->FireNow();
    pending_suggestion_timer_ = nullptr;
  }
}

void AssistiveWindowController::FocusStateChanged() {
  HideSuggestion();
  if (undo_window_)
    undo_window_->Hide();
}

void AssistiveWindowController::ShowSuggestion(
    const ui::ime::SuggestionDetails& details) {
  suggestion_text_ = details.text;
  confirmed_length_ = details.confirmed_length;
  // Delay the showing of a completion suggestion. This is required to solve
  // b/241321719, where we receive a ShowSuggestion call prior to a
  // corresponding SetBounds call. Delaying allows any relevant SetBounds calls
  // to be received before we show the suggestion to the user.
  ClearPendingSuggestionTimer();
  pending_suggestion_timer_ = std::make_unique<base::OneShotTimer>();
  pending_suggestion_timer_->Start(
      FROM_HERE, kShowSuggestionDelayMs,
      base::BindOnce(&AssistiveWindowController::DisplayCompletionSuggestion,
                     weak_ptr_factory_.GetWeakPtr(), details));
}

void AssistiveWindowController::SetButtonHighlighted(
    const ui::ime::AssistiveWindowButton& button,
    bool highlighted) {
  switch (button.window_type) {
    case ash::ime::AssistiveWindowType::kEmojiSuggestion:
    case ash::ime::AssistiveWindowType::kPersonalInfoSuggestion:
    case ash::ime::AssistiveWindowType::kMultiWordSuggestion:
    case ash::ime::AssistiveWindowType::kLongpressDiacriticsSuggestion:
      if (!suggestion_window_view_)
        return;

      suggestion_window_view_->SetButtonHighlighted(button, highlighted);
      if (highlighted)
        Announce(button.announce_string);
      break;
    case ash::ime::AssistiveWindowType::kUndoWindow:
      if (!undo_window_)
        return;

      undo_window_->SetButtonHighlighted(button, highlighted);
      Announce(button.announce_string);
      break;
    case ash::ime::AssistiveWindowType::kGrammarSuggestion:
      if (!grammar_suggestion_window_)
        return;

      grammar_suggestion_window_->SetButtonHighlighted(button, highlighted);
      if (highlighted)
        Announce(button.announce_string);
      break;
    case ash::ime::AssistiveWindowType::kNone:
      break;
  }
}

std::u16string AssistiveWindowController::GetSuggestionText() const {
  return suggestion_text_;
}

size_t AssistiveWindowController::GetConfirmedLength() const {
  return confirmed_length_;
}

ui::ime::SuggestionWindowView::Orientation
AssistiveWindowController::WindowOrientationFor(
    ash::ime::AssistiveWindowType window_type) {
  switch (window_type) {
    case ash::ime::AssistiveWindowType::kLongpressDiacriticsSuggestion:
      return ui::ime::SuggestionWindowView::Orientation::kHorizontal;
    case ash::ime::AssistiveWindowType::kUndoWindow:
    case ash::ime::AssistiveWindowType::kEmojiSuggestion:
    case ash::ime::AssistiveWindowType::kPersonalInfoSuggestion:
    case ash::ime::AssistiveWindowType::kMultiWordSuggestion:
    case ash::ime::AssistiveWindowType::kGrammarSuggestion:
      return ui::ime::SuggestionWindowView::Orientation::kVertical;
    case ash::ime::AssistiveWindowType::kNone:
      NOTREACHED();
  }
  NOTREACHED();
  return ui::ime::SuggestionWindowView::Orientation::kVertical;
}

void AssistiveWindowController::SetAssistiveWindowProperties(
    const AssistiveWindowProperties& window) {
  window_ = window;

  // Make sure any pending timers are cleared before we attempt to show, or
  // update, another assistive window.
  ClearPendingSuggestionTimer();

  switch (window.type) {
    case ash::ime::AssistiveWindowType::kUndoWindow:
      if (!undo_window_)
        InitUndoWindow();
      if (window.visible) {
        // Apply 4px padding to move the window away from the cursor.
        gfx::Rect anchor_rect =
            bounds_.autocorrect.IsEmpty() ? bounds_.caret : bounds_.autocorrect;
        anchor_rect.Inset(-4);
        undo_window_->SetAnchorRect(anchor_rect);
        undo_window_->Show();
        SetAssistiveWindow(UndoAssistiveWindow());
      } else {
        undo_window_->Hide();
        SetAssistiveWindow(HiddenAssistiveWindow());
      }
      break;
    case ash::ime::AssistiveWindowType::kEmojiSuggestion:
    case ash::ime::AssistiveWindowType::kPersonalInfoSuggestion:
    case ash::ime::AssistiveWindowType::kMultiWordSuggestion:
    case ash::ime::AssistiveWindowType::kLongpressDiacriticsSuggestion:
      if (!suggestion_window_view_)
        InitSuggestionWindow(WindowOrientationFor(window.type));
      if (window_.visible) {
        suggestion_window_view_->SetAnchorRect(bounds_.caret);
        suggestion_window_view_->ShowMultipleCandidates(
            window, WindowOrientationFor(window.type));
        if (window.suggestion_type)
          SetAssistiveWindow(MultiCandidateAssistiveWindow(
              /*window_type=*/window.type,
              /*suggestion_type=*/*window.suggestion_type,
              /*candidates=*/window.candidates));
      } else {
        HideSuggestion();
      }
      break;
    case ash::ime::AssistiveWindowType::kGrammarSuggestion:
      if (window.candidates.size() == 0)
        return;
      if (!grammar_suggestion_window_)
        InitGrammarSuggestionWindow();
      if (window.visible) {
        grammar_suggestion_window_->SetBounds(bounds_.caret);
        grammar_suggestion_window_->SetSuggestion(window.candidates[0]);
        grammar_suggestion_window_->Show();
        SetAssistiveWindow(SingleCandidateAssistiveWindow(
            /*window_type=*/AssistiveWindowType::kGrammarSuggestion,
            /*suggestion_type=*/AssistiveSuggestionType::kGrammar,
            /*candidate=*/window.candidates[0],
            /*confirmed_length=*/0));
      } else {
        grammar_suggestion_window_->Hide();
        SetAssistiveWindow(HiddenAssistiveWindow());
      }
      break;
    case ash::ime::AssistiveWindowType::kNone:
    default:
      break;
  }
  Announce(window.announce_string);
}

void AssistiveWindowController::DisplayCompletionSuggestion(
    const ui::ime::SuggestionDetails& details) {
  if (!suggestion_window_view_)
    InitSuggestionWindow(ui::ime::SuggestionWindowView::Orientation::kVertical);
  suggestion_window_view_->SetAnchorRect(bounds_.caret);
  suggestion_window_view_->Show(details);
  SetAssistiveWindow(SingleCandidateAssistiveWindow(
      /*window_type=*/ToAssistiveWindowType(details.type),
      /*suggestion_type=*/details.type,
      /*candidate=*/details.text,
      /*confirmed_length=*/details.confirmed_length));
}

void AssistiveWindowController::ClearPendingSuggestionTimer() {
  if (pending_suggestion_timer_) {
    if (pending_suggestion_timer_->IsRunning())
      pending_suggestion_timer_->Stop();
    pending_suggestion_timer_ = nullptr;
  }
}

void AssistiveWindowController::SetAssistiveWindow(
    const AssistiveWindow& window) {
  if (assistive_window_ != window)
    AssistiveWindowChanged(window);
  assistive_window_ = window;
}

void AssistiveWindowController::AssistiveWindowButtonClicked(
    const ui::ime::AssistiveWindowButton& button) const {
  delegate_->AssistiveWindowButtonClicked(button);
}

void AssistiveWindowController::AssistiveWindowChanged(
    const ash::ime::AssistiveWindow& window) const {
  delegate_->AssistiveWindowChanged(window);
}

ui::ime::SuggestionWindowView*
AssistiveWindowController::GetSuggestionWindowViewForTesting() const {
  return suggestion_window_view_;
}

ui::ime::UndoWindow* AssistiveWindowController::GetUndoWindowForTesting()
    const {
  return undo_window_;
}

ui::ime::GrammarSuggestionWindow*
AssistiveWindowController::GetGrammarWindowForTesting() const {
  return grammar_suggestion_window_;
}

}  // namespace input_method
}  // namespace ash
