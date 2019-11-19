// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_device_source.h"

#include "base/callback.h"

SharingDeviceSource::SharingDeviceSource() = default;

SharingDeviceSource::~SharingDeviceSource() = default;

void SharingDeviceSource::AddReadyCallback(base::OnceClosure callback) {
  ready_callbacks_.push_back(std::move(callback));
  MaybeRunReadyCallbacks();
}

void SharingDeviceSource::MaybeRunReadyCallbacks() {
  if (!IsReady())
    return;

  for (auto& callback : ready_callbacks_)
    std::move(callback).Run();

  ready_callbacks_.clear();
}
