// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SEARCH_BOX_SEARCH_BOX_VIEW_DELEGATE_H_
#define ASH_SEARCH_BOX_SEARCH_BOX_VIEW_DELEGATE_H_

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

  // Invoked when search box active status has changed.
  virtual void ActiveChanged(SearchBoxViewBase* sender) = 0;

  // Invoked when search box focus is changed.
  virtual void SearchBoxFocusChanged(SearchBoxViewBase* sender) = 0;

 protected:
  virtual ~SearchBoxViewDelegate() {}
};

}  // namespace ash

#endif  // ASH_SEARCH_BOX_SEARCH_BOX_VIEW_DELEGATE_H_
