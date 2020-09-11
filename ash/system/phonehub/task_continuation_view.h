// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_TASK_CONTINUATION_VIEW_H_
#define ASH_SYSTEM_PHONEHUB_TASK_CONTINUATION_VIEW_H_

#include "ash/ash_export.h"
#include "ui/views/view.h"

namespace ash {

// A view in Phone Hub bubble that allows user to pick up unfinished task left
// off from their phone, currently only support web browsing.
class ASH_EXPORT TaskContinuationView : public views::View {
 public:
  TaskContinuationView();
  ~TaskContinuationView() override;
  TaskContinuationView(TaskContinuationView&) = delete;
  TaskContinuationView operator=(TaskContinuationView&) = delete;

  // views::View:
  const char* GetClassName() const override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_TASK_CONTINUATION_VIEW_H_
