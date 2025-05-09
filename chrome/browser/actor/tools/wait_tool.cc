// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/wait_tool.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/common/actor/action_result.h"

namespace actor {

namespace {

constexpr base::TimeDelta kWaitTime = base::Seconds(3);

}  // namespace

bool WaitTool::no_delay_for_testing_ = false;

WaitTool::WaitTool() = default;

WaitTool::~WaitTool() = default;

void WaitTool::Validate(ValidateCallback callback) {
  PostResponseTask(std::move(callback), MakeOkResult());
}

void WaitTool::Invoke(InvokeCallback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&WaitTool::OnDelayFinished, weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback)),
      no_delay_for_testing_ ? base::TimeDelta() : kWaitTime);
}

std::string WaitTool::DebugString() const {
  return "WaitTool";
}

bool WaitTool::ShouldAddCompletionDelay() const {
  return false;
}

void WaitTool::OnDelayFinished(InvokeCallback callback) {
  // TODO(crbug.com/409566732): Add more robust methods for detecting that the
  // page has settled.
  std::move(callback).Run(MakeOkResult());
}

// static
void WaitTool::SetNoDelayForTesting() {
  no_delay_for_testing_ = true;
}

}  // namespace actor
