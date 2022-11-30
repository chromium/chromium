// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/scheduling/nearby_share_scheduler.h"

#include <utility>

NearbyShareScheduler::NearbyShareScheduler(OnRequestCallback callback)
    : callback_(std::move(callback)) {}

NearbyShareScheduler::~NearbyShareScheduler() = default;

void NearbyShareScheduler::Start() {
  DCHECK(!is_running_);
  is_running_ = true;
  OnStart();
}

void NearbyShareScheduler::Stop() {
  DCHECK(is_running_);
  is_running_ = false;
  OnStop();
}

void NearbyShareScheduler::NotifyOfRequest() {
  callback_.Run();
}
