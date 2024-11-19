// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_NEW_TAB_PAGE_MODULES_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_NEW_TAB_PAGE_MODULES_H_

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace signin {
class IdentityManager;
}

class Profile;

namespace ntp {

struct ModuleIdDetail {
  ModuleIdDetail(const char* id, int name_message_id)
      : id_(id), name_message_id_(name_message_id) {}
  ModuleIdDetail(const char* id,
                 int name_message_id,
                 int description_message_id)
      : id_(id),
        name_message_id_(name_message_id),
        description_message_id_(description_message_id) {}

  std::string id_;
  int name_message_id_;
  std::optional<int> description_message_id_;
};

const std::vector<ModuleIdDetail> MakeModuleIdDetails(bool is_managed_profile,
                                                      Profile* profile);

// Modules are considered enabled if there are actual modules enabled and
// account credentials are available (as most modules won't have data to
// render otherwise) or if the "--signed-out-ntp-modules" command line switch
// override is provided.
bool HasModulesEnabled(const std::vector<ModuleIdDetail> module_id_details,
                       signin::IdentityManager* identity_manager);

}  // namespace ntp

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_NEW_TAB_PAGE_MODULES_H_
