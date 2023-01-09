// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/status_collector/status_collector_state.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"

namespace policy {

StatusCollectorState::StatusCollectorState(
    const scoped_refptr<base::SequencedTaskRunner> task_runner,
    StatusCollectorCallback response)
    : task_runner_(task_runner), response_(std::move(response)) {}

StatusCollectorParams& StatusCollectorState::response_params() {
  return response_params_;
}

// Protected.
StatusCollectorState::~StatusCollectorState() {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(response_), std::move(response_params_)));
}

}  // namespace policy
