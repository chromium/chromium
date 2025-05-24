// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_NOTICE_SERVICE_INTERFACE_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_NOTICE_SERVICE_INTERFACE_H_

#include <vector>

#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/privacy_sandbox/notice/notice.mojom-forward.h"
#include "components/keyed_service/core/keyed_service.h"

namespace privacy_sandbox {

class DesktopViewManagerInterface;

enum class SurfaceType;

// This framework communicates to the view manager via this interface.
class PrivacySandboxNoticeServiceInterface : public KeyedService {
 public:
  // Returns a required list of notices to show.
  virtual std::vector<notice::mojom::PrivacySandboxNotice> GetRequiredNotices(
      SurfaceType surface) = 0;

  // Processes an |event| that occurs on a |notice_id|.
  virtual void EventOccurred(
      std::pair<notice::mojom::PrivacySandboxNotice, SurfaceType> notice_id,
      notice::mojom::PrivacySandboxNoticeEvent event) = 0;

#if !BUILDFLAG(IS_ANDROID)
  virtual DesktopViewManagerInterface* GetDesktopViewManager() = 0;
#endif  // !BUILDFLAG(IS_ANDROID)
};

}  // namespace privacy_sandbox

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_NOTICE_SERVICE_INTERFACE_H_
