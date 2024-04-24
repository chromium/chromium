// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_TASK_CONTINUATION_VIEW_H_
#define ASH_SYSTEM_PHONEHUB_TASK_CONTINUATION_VIEW_H_

#include "ash/ash_export.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/phonehub/phone_model.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"
#include "ui/views/view_model.h"

namespace ash {

namespace phonehub {
class UserActionRecorder;
}

// A view in Phone Hub bubble that allows user to pick up unfinished task left
// off from their phone, currently only support web browsing.
class ASH_EXPORT TaskContinuationView : public views::View,
                                        public phonehub::PhoneModel::Observer {
  METADATA_HEADER(TaskContinuationView, views::View)

 public:
  TaskContinuationView(phonehub::PhoneModel* phone_model,
                       phonehub::UserActionRecorder* user_action_recorder);
  ~TaskContinuationView() override;
  TaskContinuationView(TaskContinuationView&) = delete;
  TaskContinuationView operator=(TaskContinuationView&) = delete;

  // phonehub::PhoneHubModel::Observer:
  void OnModelChanged() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(TaskContinuationViewTest, TaskChipsView);

  class TaskChipsView : public views::View {
    METADATA_HEADER(TaskChipsView, views::View)

   public:
    TaskChipsView();
    ~TaskChipsView() override;
    TaskChipsView(TaskChipsView&) = delete;
    TaskChipsView operator=(TaskChipsView&) = delete;

    void AddTaskChip(views::View* task_chip);

    // views::View:
    gfx::Size CalculatePreferredSize(
        const views::SizeBounds& available_size) const override;
    void Layout(PassKey) override;

    // Clear all existing tasks in the view and in |task_chips_|.
    void Reset();

   private:
    gfx::Point GetButtonPosition(int index);
    void CalculateIdealBounds();

    views::ViewModelT<views::View> task_chips_;
  };

  // Update the chips to display current phone status.
  void Update();

  raw_ptr<phonehub::PhoneModel> phone_model_ = nullptr;
  raw_ptr<phonehub::UserActionRecorder> user_action_recorder_ = nullptr;
  raw_ptr<TaskChipsView> chips_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_TASK_CONTINUATION_VIEW_H_
