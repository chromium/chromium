// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_task_options.h"

#include "chrome/browser/actor/actor_task_delegate.h"
#include "chrome/common/actor_webui.mojom.h"

namespace actor {

ActorTaskOptions::ActorTaskOptions() = default;

ActorTaskOptions::ActorTaskOptions(ActorTaskOptions&&) = default;

ActorTaskOptions& ActorTaskOptions::operator=(ActorTaskOptions&&) = default;

ActorTaskOptions::~ActorTaskOptions() = default;

}  // namespace actor
