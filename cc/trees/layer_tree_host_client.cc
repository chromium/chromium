// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host_client.h"

namespace cc {

std::string LayerTreeHostClient::GetPausedDebuggerLocalizedMessage() {
  return "Debugger paused in another tab, click to switch to that tab.";
}

}  // namespace cc
