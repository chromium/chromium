// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/tool_callbacks.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/common/actor.mojom.h"

namespace actor {

void PostResponseTask(base::OnceCallback<void(mojom::ActionResultPtr)> task,
                      mojom::ActionResultPtr result,
                      base::TimeDelta delay) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(std::move(task), std::move(result)), delay);
}

}  // namespace actor
