// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "ash/components/arc/test/fake_timer_instance.h"

namespace arc {

FakeTimerInstance::FakeTimerInstance() = default;

FakeTimerInstance::~FakeTimerInstance() = default;

void FakeTimerInstance::Init(mojo::PendingRemote<mojom::TimerHost> host_remote,
                             InitCallback callback) {
  host_remote_.reset();
  host_remote_.Bind(std::move(host_remote));
  std::move(callback).Run();
}

mojom::TimerHost* FakeTimerInstance::GetTimerHost() const {
  return host_remote_.get();
}

}  // namespace arc
