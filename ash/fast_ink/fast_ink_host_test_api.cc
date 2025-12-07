// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/fast_ink/fast_ink_host_test_api.h"

#include "ash/fast_ink/fast_ink_host.h"

namespace ash {

FastInkHostTestApi::FastInkHostTestApi(FastInkHost* host)
    : fast_ink_host_(host) {}

FastInkHostTestApi::~FastInkHostTestApi() = default;

gpu::ClientSharedImage* FastInkHostTestApi::client_shared_image() const {
  return fast_ink_host_->client_shared_image_.get();
}

const gpu::SyncToken& FastInkHostTestApi::sync_token() const {
  return fast_ink_host_->sync_token_;
}

int FastInkHostTestApi::pending_bitmaps_size() const {
  return fast_ink_host_->pending_bitmaps_.size();
}

}  // namespace ash
