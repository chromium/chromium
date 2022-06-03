// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_auto_opener.h"

#include "base/command_line.h"
#include "chrome/browser/devtools/devtools_window.h"

DevToolsAutoOpener::DevToolsAutoOpener()
    : browser_tab_strip_tracker_(this, nullptr) {
  browser_tab_strip_tracker_.Init();
}

DevToolsAutoOpener::~DevToolsAutoOpener() {
}

void DevToolsAutoOpener::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() != TabStripModelChange::kInserted)
    return;

  for (const auto& contents : change.GetInsert()->contents)
    if (!DevToolsWindow::IsDevToolsWindow(contents.contents))
      DevToolsWindow::OpenDevToolsWindow(contents.contents);
}
