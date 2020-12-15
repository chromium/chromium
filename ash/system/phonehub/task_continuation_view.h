// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_TASK_CONTINUATION_VIEW_H_
#define ASH_SYSTEM_PHONEHUB_TASK_CONTINUATION_VIEW_H_

#include "ash/ash_export.h"
#include "chromeos/components/phonehub/phone_model.h"
#include "ui/views/view.h"
#include "ui/views/view_model.h"

namespace chromeos {
namespace phonehub {
class UserActionRecorder;
}  // namespace phonehub
}  // namespace chromeos

namespace ash {

// A view in Phone Hub bubble that allows user to pick up unfinished task left
// off from their phone, currently only support web browsing.
class ASH_EXPORT TaskContinuationView
    : public views::View,
      public chromeos::phonehub::PhoneModel::Observer {
 public:
  TaskContinuationView(
      chromeos::phonehub::PhoneModel* phone_model,
      chromeos::phonehub::UserActionRecorder* user_action_recorder);
  ~TaskContinuationView() override;
  TaskContinuationView(TaskContinuationView&) = delete;
  TaskContinuationView operator=(TaskContinuationView&) = delete;

  // chromeos::phonehub::PhoneHubModel::Observer:
  void OnModelChanged() override;

  // views::View:
  const char* GetClassName() const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(TaskContinuationViewTest, TaskChipsView);

  class TaskChipsView : public views::View {
   public:
    TaskChipsView();
    ~TaskChipsView() override;
    TaskChipsView(TaskChipsView&) = delete;
    TaskChipsView operator=(TaskChipsView&) = delete;

    void AddTaskChip(views::View* task_chip);

    // views::View:
    gfx::Size CalculatePreferredSize() const override;
    void Layout() override;
    const char* GetClassName() const override;

    // Clear all existing tasks in the view and in |task_chips_|.
    void Reset();

   private:
    gfx::Point GetButtonPosition(int index);
    void CalculateIdealBounds();

    views::ViewModelT<views::View> task_chips_;
  };

  // Update the chips to display current phone status.
  void Update();

  chromeos::phonehub::PhoneModel* phone_model_ = nullptr;
  chromeos::phonehub::UserActionRecorder* user_action_recorder_ = nullptr;
  TaskChipsView* chips_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_TASK_CONTINUATION_VIEW_H_
