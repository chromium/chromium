// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_task.h"

namespace actor {

ActorTask::ActorTask() = default;
ActorTask::~ActorTask() = default;

ActorTask::State ActorTask::GetState() const {
  return state_;
}

}  // namespace actor
