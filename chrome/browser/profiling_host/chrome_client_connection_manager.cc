// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiling_host/chrome_client_connection_manager.h"

#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_process_host.h"

namespace heap_profiling {

ChromeClientConnectionManager::ChromeClientConnectionManager(
    base::WeakPtr<Controller> controller,
    Mode mode)
    : ClientConnectionManager(controller, mode) {}

bool ChromeClientConnectionManager::AllowedToProfileRenderer(
    content::RenderProcessHost* host) {
  return !host->GetBrowserContext()->IsOffTheRecord();
}

}  // namespace heap_profiling
