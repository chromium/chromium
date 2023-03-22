// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/slim/simple_scheduler.h"

namespace cc::slim {

SimpleScheduler::SimpleScheduler() = default;
SimpleScheduler::~SimpleScheduler() = default;

void SimpleScheduler::Initialize(SchedulerClient* client) {
  DCHECK(!client_);
  DCHECK(client);
  client_ = client;
}

void SimpleScheduler::OnBeginFrameFromViz(
    const viz::BeginFrameArgs& begin_frame_args) {
  if (!client_->DoBeginFrame(begin_frame_args)) {
    client_->SendDidNotProduceFrame(begin_frame_args);
  }
}

}  // namespace cc::slim
