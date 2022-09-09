// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_DID_RUN_UPDATER_WIN_H_
#define CHROME_BROWSER_GOOGLE_DID_RUN_UPDATER_WIN_H_

#include "content/public/browser/render_process_host_creation_observer.h"

// Updates Chrome's "did run" state periodically when the process is in use.
// The creation of renderers is used as a proxy for "is the browser in use."
class DidRunUpdater final : public content::RenderProcessHostCreationObserver {
 public:
  DidRunUpdater() = default;
  DidRunUpdater(const DidRunUpdater&) = delete;
  DidRunUpdater& operator=(const DidRunUpdater&) = delete;

  // content::RenderProcessHostCreationObserver:
  void OnRenderProcessHostCreated(
      content::RenderProcessHost* process_host) override;
};

#endif  // CHROME_BROWSER_GOOGLE_DID_RUN_UPDATER_WIN_H_
