// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILING_HOST_CHROME_CLIENT_CONNECTION_MANAGER_H_
#define CHROME_BROWSER_PROFILING_HOST_CHROME_CLIENT_CONNECTION_MANAGER_H_

#include "components/heap_profiling/multi_process/client_connection_manager.h"

namespace heap_profiling {

// This class overrides ClientConnectionManager in order to prevent incognito
// and guest mode renderers from being profiled.
// Like ClientConnectionManager, this class must only be constructed/accessed
// from the UI thread.
class ChromeClientConnectionManager : public ClientConnectionManager {
 public:
  ChromeClientConnectionManager(base::WeakPtr<Controller> controller,
                                Mode mode);

  ChromeClientConnectionManager(const ChromeClientConnectionManager&) = delete;
  ChromeClientConnectionManager& operator=(
      const ChromeClientConnectionManager&) = delete;

  bool AllowedToProfileRenderer(content::RenderProcessHost* host) override;
};

}  // namespace heap_profiling

#endif  // CHROME_BROWSER_PROFILING_HOST_CHROME_CLIENT_CONNECTION_MANAGER_H_
