// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/webrtc_event_log_manager_keyed_service.h"

#include "base/check.h"
#include "base/functional/callback.h"
#include "chrome/browser/media/webrtc/webrtc_event_log_manager.h"
#include "content/public/browser/browser_context.h"

namespace webrtc_event_logging {

WebRtcEventLogManagerKeyedService::WebRtcEventLogManagerKeyedService(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  DCHECK(!browser_context_->IsOffTheRecord());

  WebRtcEventLogManager* manager = WebRtcEventLogManager::GetInstance();
  if (manager) {
    manager->EnableForBrowserContext(browser_context_, base::OnceClosure());
    reported_ = true;
  } else {
    reported_ = false;
  }
}

void WebRtcEventLogManagerKeyedService::Shutdown() {
  WebRtcEventLogManager* manager = WebRtcEventLogManager::GetInstance();
  if (manager) {
    DCHECK(reported_) << "WebRtcEventLogManager constructed too late.";
    manager->DisableForBrowserContext(browser_context_, base::OnceClosure());
  }
}

}  // namespace webrtc_event_logging
