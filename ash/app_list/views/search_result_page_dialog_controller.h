// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SEARCH_RESULT_PAGE_DIALOG_CONTROLLER_H_
#define ASH_APP_LIST_VIEWS_SEARCH_RESULT_PAGE_DIALOG_CONTROLLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"

namespace views {
class WidgetDelegate;
class View;
}  // namespace views

namespace ash {

class SearchResultPageAnchoredDialog;

// Controller that can be used to show a dialog anchored within the app list
// search UI.
class SearchResultPageDialogController {
 public:
  explicit SearchResultPageDialogController(views::View* host_view);
  SearchResultPageDialogController(const SearchResultPageDialogController&) =
      delete;
  SearchResultPageDialogController& operator=(
      const SearchResultPageDialogController&) = delete;
  ~SearchResultPageDialogController();

  // Shows a search results page dialog with contents `dialog_contents`.
  // No-op if not enabled.
  void Show(std::unique_ptr<views::WidgetDelegate> dialog_contents);

  // Sets whether search result page dialogs are enabled. It closes the
  // current dialog if it exists.
  void Reset(bool enabled);

  SearchResultPageAnchoredDialog* dialog() { return dialog_.get(); }

 private:
  // Called when the search result page dialog gets closed.
  void OnAnchoredDialogClosed();

  const raw_ptr<views::View> host_view_;

  // Whether search result page dialogs are allowed. If false, calls to `Show()`
  // will be no-op.
  bool enabled_ = false;

  // Currently shown dialog - null when no dialog is shown.
  std::unique_ptr<SearchResultPageAnchoredDialog> dialog_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_SEARCH_RESULT_PAGE_DIALOG_CONTROLLER_H_
