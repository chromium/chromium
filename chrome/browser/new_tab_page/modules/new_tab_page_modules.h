// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_NEW_TAB_PAGE_MODULES_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_NEW_TAB_PAGE_MODULES_H_

#include <string>
#include <utility>
#include <vector>

namespace ntp {

const std::vector<std::pair<const std::string, int>> MakeModuleIdNames(
    bool drive_module_enabled);

}  // namespace ntp

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_NEW_TAB_PAGE_MODULES_H_
