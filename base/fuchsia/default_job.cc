// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/default_job.h"

#include <zircon/types.h>

#include "base/check_op.h"

namespace base {

namespace {
zx_handle_t g_job = ZX_HANDLE_INVALID;
}

zx::unowned_job GetDefaultJob() {
  if (g_job == ZX_HANDLE_INVALID)
    return zx::job::default_job();
  return zx::unowned_job(g_job);
}

void SetDefaultJob(zx::job job) {
  DCHECK_EQ(g_job, ZX_HANDLE_INVALID);
  g_job = job.release();
}

ScopedDefaultJobForTest::ScopedDefaultJobForTest(zx::job new_default_job) {
  DCHECK(new_default_job.is_valid());
  old_default_job_.reset(g_job);
  g_job = new_default_job.release();
}

ScopedDefaultJobForTest::~ScopedDefaultJobForTest() {
  DCHECK_NE(g_job, ZX_HANDLE_INVALID);
  zx::job my_default_job(g_job);
  g_job = old_default_job_.release();
}

}  // namespace base
