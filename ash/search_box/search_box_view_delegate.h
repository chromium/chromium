// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SEARCH_BOX_SEARCH_BOX_VIEW_DELEGATE_H_
#define ASH_SEARCH_BOX_SEARCH_BOX_VIEW_DELEGATE_H_

namespace ui {
class KeyEvent;
}  // namespace ui

namespace ash {

class SearchBoxViewBase;

class SearchBoxViewDelegate {
 public:
  // Invoked when query text has changed by the user.
  virtual void QueryChanged(SearchBoxViewBase* sender) = 0;

  // Invoked when the back button has been pressed.
  virtual void AssistantButtonPressed() = 0;

  // Invoked when the back button has been pressed.
  virtual void BackButtonPressed() = 0;

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

#endif  // ASH_SEARCH_BOX_SEARCH_BOX_VIEW_DELEGATE_H_
