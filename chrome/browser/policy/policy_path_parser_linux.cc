// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <pwd.h>
#include <stddef.h>
#include <sys/types.h>
#include <unistd.h>

#include "chrome/browser/policy/policy_path_parser.h"

#include "base/logging.h"

namespace policy {

namespace path_parser {

const char kMachineNamePolicyVarName[] = "${machine_name}";
const char kUserNamePolicyVarName[] = "${user_name}";

// Replaces all variable occurrences in the policy string with the respective
// system settings values.
base::FilePath::StringType ExpandPathVariables(
    const base::FilePath::StringType& untranslated_string) {
  base::FilePath::StringType result(untranslated_string);
  if (result.length() == 0)
    return result;
  // Sanitize quotes in case of any around the whole string.
  if (result.length() > 1 &&
      ((result.front() == '"' && result.back() == '"') ||
       (result.front() == '\'' && result.back() == '\''))) {
    // Strip first and last char which should be matching quotes now.
    result = result.substr(1, result.length() - 2);
  }
  // Translate two special variables ${user_name} and ${machine_name}
  size_t position = result.find(kUserNamePolicyVarName);
  if (position != std::string::npos) {
    struct passwd* user = getpwuid(geteuid());
    if (user) {
      result.replace(position, strlen(kUserNamePolicyVarName), user->pw_name);
    } else {
      LOG(ERROR) << "Username variable can not be resolved. ";
    }
  }
  position = result.find(kMachineNamePolicyVarName);
  if (position != std::string::npos) {
    char machinename[255];
    if (gethostname(machinename, 255) == 0) {
      result.replace(position, strlen(kMachineNamePolicyVarName), machinename);
    } else {
      LOG(ERROR) << "Machine name variable can not be resolved.";
    }
  }
  return result;
}

void CheckUserDataDirPolicy(base::FilePath* user_data_dir) {
  // This function is not implemented in Linux because we don't support the
  // policy on this platform.
  NOTREACHED();
}

}  // namespace path_parser

}  // namespace policy
