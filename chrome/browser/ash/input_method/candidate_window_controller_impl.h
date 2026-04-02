// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_CANDIDATE_WINDOW_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_CANDIDATE_WINDOW_CONTROLLER_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ash/input_method/candidate_window_controller.h"
#include "chrome/browser/ui/ash/input_method/candidate_window_view.h"
#include "ui/base/ime/ash/ime_candidate_window_handler_interface.h"
#include "ui/base/ime/infolist_entry.h"
#include "ui/views/widget/widget_observer.h"

namespace ui {
class CandidateWindow;

namespace ime {
class InfolistWindow;
}  // namespace ime
}  // namespace ui

namespace views {
class Widget;
}  // namespace views

namespace ash {
namespace input_method {

// The implementation of CandidateWindowController.
// CandidateWindowController controls the CandidateWindow.
class CandidateWindowControllerImpl
    : public CandidateWindowController,
      public ui::ime::CandidateWindowView::Observer,
      public IMECandidateWindowHandlerInterface {
 public:
  CandidateWindowControllerImpl();

  CandidateWindowControllerImpl(const CandidateWindowControllerImpl&) = delete;
  CandidateWindowControllerImpl& operator=(
      const CandidateWindowControllerImpl&) = delete;

  ~CandidateWindowControllerImpl() override;

  // CandidateWindowController overrides:
  void AddObserver(CandidateWindowController::Observer* observer) override;
  void RemoveObserver(CandidateWindowController::Observer* observer) override;
  void Hide() override;

 protected:
  static void ConvertLookupTableToInfolistEntry(
      const ui::CandidateWindow& candidate_window,
      std::vector<ui::InfolistEntry>* infolist_entries,
      bool* has_highlighted);

 private:
  // ui::ime::CandidateWindowView::Observer implementation.
  void OnCandidateCommitted(int index) override;


  // IMECandidateWindowHandlerInterface implementation.
  void SetCursorAndCompositionBounds(
      const gfx::Rect& cursor_bounds,
      const gfx::Rect& composition_bounds) override;
  gfx::Rect GetCursorBounds() const override;
  void HideLookupTable() override;
  void UpdateLookupTable(const ui::CandidateWindow& candidate_window) override;
  void UpdatePreeditText(const std::u16string& text,
                         unsigned int cursor,
                         bool visible) override;
  void FocusStateChanged(bool is_focused) override;

  void InitCandidateWindowView();

  std::unique_ptr<views::Widget> candidate_window_widget_;
  // The candidate window view.
  raw_ptr<ui::ime::CandidateWindowView> candidate_window_view_ = nullptr;

  std::unique_ptr<views::Widget> infolist_window_widget_;
  // This is the outer frame of the infolist window view. Owned by the widget.
  raw_ptr<ui::ime::InfolistWindow> infolist_window_ = nullptr;

  bool is_focused_ = false;

  gfx::Rect cursor_bounds_;
  gfx::Rect composition_bounds_;

  // The infolist entries and its focused index which currently shown in
  // Infolist window.
  std::vector<ui::InfolistEntry> latest_infolist_entries_;

  base::ObserverList<CandidateWindowController::Observer>::Unchecked observers_;
};

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_CANDIDATE_WINDOW_CONTROLLER_IMPL_H_
