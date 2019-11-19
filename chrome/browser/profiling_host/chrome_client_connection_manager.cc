// Copyright 2018 The Chromium Authors. All rights reserved.
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
  return Profile::FromBrowserContext(host->GetBrowserContext())
      ->IsRegularProfile();
}

}  // namespace heap_profiling
