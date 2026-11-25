// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/selection/page_selection.h"

#include "components/tabs/public/tab_interface.h"

namespace selection {

DEFINE_USER_DATA(PageSelection);

// static
PageSelection* PageSelection::From(tabs::TabInterface* tab) {
  return tab ? Get(tab->GetUnownedUserDataHost()) : nullptr;
}

}  // namespace selection
