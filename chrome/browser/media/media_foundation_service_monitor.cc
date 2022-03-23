// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_foundation_service_monitor.h"

#include "base/logging.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cdm_registry.h"
#include "media/mojo/mojom/media_foundation_service.mojom.h"

const int kMaxNumberFailures = 2;

// static
MediaFoundationServiceMonitor* MediaFoundationServiceMonitor::GetInstance() {
  static auto* monitor = new MediaFoundationServiceMonitor();
  return monitor;
}

MediaFoundationServiceMonitor::MediaFoundationServiceMonitor() {
  DVLOG(1) << __func__;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::ServiceProcessHost::AddObserver(this);
}

MediaFoundationServiceMonitor::~MediaFoundationServiceMonitor() = default;

void MediaFoundationServiceMonitor::OnServiceProcessCrashed(
    const content::ServiceProcessInfo& info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Only interested in MediaFoundationService process.
  if (!info.IsService<media::mojom::MediaFoundationServiceBroker>())
    return;

  DLOG(ERROR) << __func__ << ": MediaFoundationService process crashed!";

  num_crashes_++;

  if (num_crashes_ >= kMaxNumberFailures) {
    content::CdmRegistry::GetInstance()->DisableHardwareSecureCdms();
  }
}
