// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FAST_INK_FAST_INK_HOST_TEST_API_H_
#define ASH_FAST_INK_FAST_INK_HOST_TEST_API_H_

#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/common/sync_token.h"

namespace gpu {
class ClientSharedImage;
}  // namespace gpu

namespace ash {

class FastInkHost;

class FastInkHostTestApi {
 public:
  explicit FastInkHostTestApi(FastInkHost* host);

  FastInkHostTestApi(const FastInkHostTestApi&) = delete;
  FastInkHostTestApi& operator=(const FastInkHostTestApi&) = delete;

  ~FastInkHostTestApi();

  gpu::ClientSharedImage* client_shared_image() const;
  const gpu::SyncToken& sync_token() const;
  int pending_bitmaps_size() const;

 private:
  raw_ptr<FastInkHost> fast_ink_host_;
};

}  // namespace ash

#endif  // ASH_FAST_INK_FAST_INK_HOST_TEST_API_H_
