// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_CANDIDATE_WINDOW_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_CANDIDATE_WINDOW_CONTROLLER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/input_method/candidate_window_controller.h"
#include "ui/base/ime/ime_candidate_window_handler_interface.h"
#include "ui/base/ime/infolist_entry.h"
#include "ui/chromeos/ime/candidate_window_view.h"
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

namespace chromeos {
namespace input_method {

// The implementation of CandidateWindowController.
// CandidateWindowController controls the CandidateWindow.
class CandidateWindowControllerImpl
    : public CandidateWindowController,
      public ui::ime::CandidateWindowView::Observer,
      public views::WidgetObserver,
      public IMECandidateWindowHandlerInterface {
 public:
  CandidateWindowControllerImpl();
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

  // views::WidgetObserver implementation.
  void OnWidgetClosing(views::Widget* widget) override;

  // IMECandidateWindowHandlerInterface implementation.
  void SetCursorBounds(const gfx::Rect& cursor_bounds,
                       const gfx::Rect& composition_head) override;
  gfx::Rect GetCursorBounds() const override;
  void UpdateLookupTable(const ui::CandidateWindow& candidate_window,
                         bool visible) override;
  void UpdatePreeditText(const base::string16& text,
                         unsigned int cursor,
                         bool visible) override;
  void FocusStateChanged(bool is_focused) override;

  void InitCandidateWindowView();

  // The candidate window view.
  ui::ime::CandidateWindowView* candidate_window_view_ = nullptr;

  // This is the outer frame of the infolist window view. Owned by the widget.
  ui::ime::InfolistWindow* infolist_window_ = nullptr;

  bool is_focused_ = false;

  gfx::Rect cursor_bounds_;
  gfx::Rect composition_head_;

  // The infolist entries and its focused index which currently shown in
  // Infolist window.
  std::vector<ui::InfolistEntry> latest_infolist_entries_;

  base::ObserverList<CandidateWindowController::Observer>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(CandidateWindowControllerImpl);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_CANDIDATE_WINDOW_CONTROLLER_IMPL_H_
