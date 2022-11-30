// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SEARCH_RESULT_ACTIONS_VIEW_DELEGATE_H_
#define ASH_APP_LIST_VIEWS_SEARCH_RESULT_ACTIONS_VIEW_DELEGATE_H_

#include <stddef.h>

namespace ash {

class SearchResultActionsViewDelegate {
 public:
  // Invoked when the action button represent the action at |index| is pressed
  // in SearchResultActionsView.
  virtual void OnSearchResultActionActivated(size_t index) = 0;

  // Returns true if the associated search result is hovered by mouse, or
  // or selected by keyboard.
  virtual bool IsSearchResultHoveredOrSelected() = 0;

 protected:
  virtual ~SearchResultActionsViewDelegate() {}
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_SEARCH_RESULT_ACTIONS_VIEW_DELEGATE_H_
