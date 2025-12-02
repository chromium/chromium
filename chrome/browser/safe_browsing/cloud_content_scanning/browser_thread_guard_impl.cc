// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/browser_thread_guard_impl.h"

#include "content/public/browser/browser_thread.h"

namespace safe_browsing {

void BrowserThreadGuardImpl::AssertCalledOnUIThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

}  // namespace safe_browsing
