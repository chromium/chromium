// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/candidate_window_controller_impl.h"

#include <string>
#include <vector>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"           // mash-ok
#include "ash/wm/window_util.h"  // mash-ok
#include "base/logging.h"
#include "ui/base/ime/ime_bridge.h"
#include "ui/chromeos/ime/infolist_window.h"
#include "ui/views/widget/widget.h"

namespace chromeos {
namespace input_method {

namespace {

}  // namespace

CandidateWindowControllerImpl::CandidateWindowControllerImpl() {
  ui::IMEBridge::Get()->SetCandidateWindowHandler(this);
}

CandidateWindowControllerImpl::~CandidateWindowControllerImpl() {
  ui::IMEBridge::Get()->SetCandidateWindowHandler(nullptr);
  if (candidate_window_view_) {
    candidate_window_view_->RemoveObserver(this);
    candidate_window_view_->GetWidget()->RemoveObserver(this);
  }
}

void CandidateWindowControllerImpl::InitCandidateWindowView() {
  if (candidate_window_view_)
    return;

  gfx::NativeView parent = nullptr;

  aura::Window* active_window = ash::window_util::GetActiveWindow();
  // Use VirtualKeyboardContainer so that it works even with a system modal
  // dialog.
  parent = ash::Shell::GetContainer(
      active_window ? active_window->GetRootWindow()
                    : ash::Shell::GetRootWindowForNewWindows(),
      ash::kShellWindowId_VirtualKeyboardContainer);
  candidate_window_view_ = new ui::ime::CandidateWindowView(
      parent, ash::kShellWindowId_VirtualKeyboardContainer);
  candidate_window_view_->AddObserver(this);
  candidate_window_view_->SetCursorBounds(cursor_bounds_, composition_head_);
  views::Widget* widget = candidate_window_view_->InitWidget();
  widget->AddObserver(this);
  widget->Show();
  for (auto& observer : observers_)
    observer.CandidateWindowOpened();
}

void CandidateWindowControllerImpl::Hide() {
  if (candidate_window_view_)
    candidate_window_view_->GetWidget()->Close();
  if (infolist_window_)
    infolist_window_->HideImmediately();
}

void CandidateWindowControllerImpl::SetCursorBounds(
    const gfx::Rect& cursor_bounds,
    const gfx::Rect& composition_head) {
  // A workaround for http://crosbug.com/6460. We should ignore very short Y
  // move to prevent the window from shaking up and down.
  const int kKeepPositionThreshold = 2;  // px
  gfx::Rect last_bounds;
  if (candidate_window_view_)
    last_bounds = candidate_window_view_->GetAnchorRect();

  const int delta_y = abs(last_bounds.y() - cursor_bounds.y());
  if ((last_bounds.x() == cursor_bounds.x()) &&
      (delta_y <= kKeepPositionThreshold)) {
    DVLOG(1) << "Ignored set_cursor_bounds signal to prevent window shake";
    return;
  }

  cursor_bounds_ = cursor_bounds;
  composition_head_ = composition_head;

  // Remember the cursor bounds.
  if (candidate_window_view_)
    candidate_window_view_->SetCursorBounds(cursor_bounds, composition_head);
}

gfx::Rect CandidateWindowControllerImpl::GetCursorBounds() const {
  return is_focused_ ? cursor_bounds_ : gfx::Rect();
}

void CandidateWindowControllerImpl::FocusStateChanged(bool is_focused) {
  is_focused_ = is_focused;
  if (candidate_window_view_)
    candidate_window_view_->HidePreeditText();
}

void CandidateWindowControllerImpl::UpdateLookupTable(
    const ui::CandidateWindow& candidate_window,
    bool visible) {
  // If it's not visible, hide the lookup table and return.
  if (!visible) {
    if (candidate_window_view_)
      candidate_window_view_->HideLookupTable();
    if (infolist_window_)
      infolist_window_->HideImmediately();
    // TODO(nona): Introduce unittests for crbug.com/170036.
    latest_infolist_entries_.clear();
    return;
  }

  if (!candidate_window_view_)
    InitCandidateWindowView();
  candidate_window_view_->UpdateCandidates(candidate_window);
  candidate_window_view_->ShowLookupTable();

  bool has_highlighted = false;
  std::vector<ui::InfolistEntry> infolist_entries;
  candidate_window.GetInfolistEntries(&infolist_entries, &has_highlighted);

  // If there is no change, just return.
  if (latest_infolist_entries_ == infolist_entries)
    return;

  latest_infolist_entries_ = infolist_entries;

  if (infolist_entries.empty()) {
    if (infolist_window_)
      infolist_window_->HideImmediately();
    return;
  }

  // Highlight moves out of the infolist entries.
  if (!has_highlighted) {
    if (infolist_window_)
      infolist_window_->HideWithDelay();
    return;
  }

  if (infolist_window_) {
    infolist_window_->Relayout(infolist_entries);
  } else {
    infolist_window_ = new ui::ime::InfolistWindow(
        candidate_window_view_, infolist_entries);
    infolist_window_->InitWidget();
    infolist_window_->GetWidget()->AddObserver(this);
  }
  infolist_window_->ShowWithDelay();
}

void CandidateWindowControllerImpl::UpdatePreeditText(
    const base::string16& text, unsigned int cursor, bool visible) {
  // If it's not visible, hide the preedit text and return.
  if (!visible || text.empty()) {
    if (candidate_window_view_)
      candidate_window_view_->HidePreeditText();
    return;
  }
  if (!candidate_window_view_)
    InitCandidateWindowView();
  candidate_window_view_->UpdatePreeditText(text);
  candidate_window_view_->ShowPreeditText();
}

void CandidateWindowControllerImpl::OnCandidateCommitted(int index) {
  for (auto& observer : observers_)
    observer.CandidateClicked(index);
}

void CandidateWindowControllerImpl::OnWidgetClosing(views::Widget* widget) {
  if (infolist_window_ && widget == infolist_window_->GetWidget()) {
    widget->RemoveObserver(this);
    infolist_window_ = nullptr;
  } else if (candidate_window_view_ &&
             widget == candidate_window_view_->GetWidget()) {
    widget->RemoveObserver(this);
    candidate_window_view_->RemoveObserver(this);
    candidate_window_view_ = nullptr;
    for (auto& observer : observers_)
      observer.CandidateWindowClosed();
  }
}

void CandidateWindowControllerImpl::AddObserver(
    CandidateWindowController::Observer* observer) {
  observers_.AddObserver(observer);
}

void CandidateWindowControllerImpl::RemoveObserver(
    CandidateWindowController::Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace input_method
}  // namespace chromeos
