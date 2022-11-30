// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/did_run_updater_win.h"

#include "chrome/installer/util/update_did_run_state.h"

void DidRunUpdater::OnRenderProcessHostCreated(
    content::RenderProcessHost* process_host) {
  installer::UpdateDidRunState(true);
}
