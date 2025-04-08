// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_NOTICE_SERVICE_INTERFACE_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_NOTICE_SERVICE_INTERFACE_H_

#include <vector>

#include "chrome/browser/privacy_sandbox/notice/notice.mojom-forward.h"


namespace privacy_sandbox {

enum class SurfaceType;

// This framework communicates to the view manager via this interface.
class PrivacySandboxNoticeServiceInterface {
 public:
  virtual ~PrivacySandboxNoticeServiceInterface() = default;

  // Returns a required list of notices to show.
  virtual std::vector<notice::mojom::PrivacySandboxNotice> GetRequiredNotices(
      SurfaceType surface) = 0;

  // Processes an |event| that occurs on a |notice_id|.
  virtual void EventOccurred(
      std::pair<notice::mojom::PrivacySandboxNotice, SurfaceType> notice_id,
      notice::mojom::PrivacySandboxNoticeEvent event) = 0;
};

}  // namespace privacy_sandbox

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_NOTICE_SERVICE_INTERFACE_H_
