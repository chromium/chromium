// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SEARCH_BOX_VIEW_DELEGATE_H_
#define ASH_APP_LIST_VIEWS_SEARCH_BOX_VIEW_DELEGATE_H_

#include <string>

namespace ui {
class KeyEvent;
}  // namespace ui

namespace ash {

class SearchBoxViewBase;

class SearchBoxViewDelegate {
 public:
  // Invoked when query text in the search box changes, just before initiating
  // search request for the query.
  // `trimmed_query` - the search boxt textfiled contents with whitespace
  // trimmed (which will generally match the query sent to search providers).
  // `initiated_by_user` - whether the query changed as a result of user input
  // (as opposed to the search box getting cleared).
  virtual void QueryChanged(const std::u16string& trimmed_query,
                            bool initiated_by_user) = 0;

  // Invoked when the back button has been pressed.
  virtual void AssistantButtonPressed() = 0;

  // Invoked when the close button has been pressed. Implementations should
  // clear the search box, but may or may not want to take focus.
  virtual void CloseButtonPressed() = 0;

  // Invoked when search box active status has changed.
  virtual void ActiveChanged(SearchBoxViewBase* sender) = 0;

  // Invoked for key events on the search box itself (e.g. arrow keys when one
  // of the buttons is focused).
  virtual void OnSearchBoxKeyEvent(ui::KeyEvent* event) = 0;

  // Returns true if search results can be selected with the keyboard (e.g.
  // search results exist and are visible to the user).
  virtual bool CanSelectSearchResults() = 0;

 protected:
  virtual ~SearchBoxViewDelegate() = default;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_SEARCH_BOX_VIEW_DELEGATE_H_
