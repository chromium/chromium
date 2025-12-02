// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_BROWSER_THREAD_GUARD_IMPL_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_BROWSER_THREAD_GUARD_IMPL_H_

#include "components/enterprise/connectors/core/cloud_content_scanning/browser_thread_guard.h"

namespace safe_browsing {

class BrowserThreadGuardImpl
    : public enterprise_connectors::BrowserThreadGuard {
 public:
  void AssertCalledOnUIThread() override;
  ~BrowserThreadGuardImpl() override = default;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_BROWSER_THREAD_GUARD_IMPL_H_
