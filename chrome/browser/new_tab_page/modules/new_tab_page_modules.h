// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_NEW_TAB_PAGE_MODULES_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_NEW_TAB_PAGE_MODULES_H_

#include <string>
#include <utility>
#include <vector>

namespace signin {
class IdentityManager;
}

class Profile;

namespace ntp {

const std::vector<std::pair<const std::string, int>> MakeModuleIdNames(
    bool is_managed_profile,
    Profile* profile);

// Modules are considered enabled if there are actual modules enabled and
// account credentials are available (as most modules won't have data to
// render otherwise) or if the "--signed-out-ntp-modules" command line switch
// override is provided.
bool HasModulesEnabled(
    std::vector<std::pair<const std::string, int>> module_id_names,
    signin::IdentityManager* identity_manager);

}  // namespace ntp

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_NEW_TAB_PAGE_MODULES_H_
