// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/common/glic_tab_util.h"

#include "chrome/browser/tab_list/tab_list_interface.h"

namespace glic {

tabs::TabInterface* GetActiveTab(BrowserWindowInterface* browser) {
  TabListInterface* tab_list = TabListInterface::From(browser);
  return tab_list ? tab_list->GetActiveTab() : nullptr;
}

}  // namespace glic
