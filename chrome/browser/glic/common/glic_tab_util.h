// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_COMMON_GLIC_TAB_UTIL_H_
#define CHROME_BROWSER_GLIC_COMMON_GLIC_TAB_UTIL_H_

class BrowserWindowInterface;

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace glic {

// Returns the active tab in `browser`, or nullptr if `browser` or its
// TabListInterface is null.
tabs::TabInterface* GetActiveTab(BrowserWindowInterface* browser);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_COMMON_GLIC_TAB_UTIL_H_
