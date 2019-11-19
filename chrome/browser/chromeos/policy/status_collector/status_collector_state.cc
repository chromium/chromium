// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/status_collector/status_collector_state.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"

namespace policy {

StatusCollectorState::StatusCollectorState(
    const scoped_refptr<base::SequencedTaskRunner> task_runner,
    const StatusCollectorCallback& response)
    : task_runner_(task_runner), response_(response) {}

StatusCollectorParams& StatusCollectorState::response_params() {
  return response_params_;
}

// Protected.
StatusCollectorState::~StatusCollectorState() {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(response_, base::Passed(&response_params_)));
}

}  // namespace policy
