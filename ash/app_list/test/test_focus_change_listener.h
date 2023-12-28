// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_TEST_TEST_FOCUS_CHANGE_LISTENER_H_
#define ASH_APP_LIST_TEST_TEST_FOCUS_CHANGE_LISTENER_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/focus/focus_manager.h"

namespace ash {

// A helper class to observe focus changes on the specified focus manager.
class TestFocusChangeListener : public views::FocusChangeListener {
 public:
  explicit TestFocusChangeListener(views::FocusManager* focus_manager);
  TestFocusChangeListener(const TestFocusChangeListener&) = delete;
  TestFocusChangeListener& operator=(const TestFocusChangeListener&) = delete;
  ~TestFocusChangeListener() override;

  // views::FocusChangeListener:
  void OnWillChangeFocus(views::View* focused_before,
                         views::View* focused_now) override {}
  void OnDidChangeFocus(views::View* focused_before,
                        views::View* focused_now) override;

  int focus_change_count() { return focus_change_count_; }

 private:
  const raw_ptr<views::FocusManager> focus_manager_;

  // Records the count of focus changes.
  int focus_change_count_ = 0;
};

}  // namespace ash

#endif  // ASH_APP_LIST_TEST_TEST_FOCUS_CHANGE_LISTENER_H_
