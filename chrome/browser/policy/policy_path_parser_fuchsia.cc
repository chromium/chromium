// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_path_parser.h"

#include <string>

#include "base/logging.h"
#include "base/notreached.h"

namespace policy {

namespace path_parser {

const char kMachineNamePolicyVarName[] = "${machine_name}";
const char kUserNamePolicyVarName[] = "${user_name}";

base::FilePath::StringType ExpandPathVariables(
    const base::FilePath::StringType& untranslated_string) {
  base::FilePath::StringType result(untranslated_string);
  if (result.length() == 0)
    return result;

  // Policy paths may be wrapped in quotes, which should be removed.
  if (result.length() > 1 &&
      ((result.front() == '"' && result.back() == '"') ||
       (result.front() == '\'' && result.back() == '\''))) {
    // Strip first and last char which should be matching quotes now.
    result = result.substr(1, result.length() - 2);
  }

  // Translate two special variables ${user_name} and ${machine_name}
  // TODO(crbug.com/1231482): Integrate with platform provided values, as
  // they become available.
  size_t position = result.find(kUserNamePolicyVarName);
  if (position != std::string::npos) {
    NOTIMPLEMENTED() << "Username variable not implemented.";
    result.replace(position, strlen(kUserNamePolicyVarName), "user");
  }
  position = result.find(kMachineNamePolicyVarName);
  if (position != std::string::npos) {
    NOTIMPLEMENTED() << "Machine name variable not implemented.";
    result.replace(position, strlen(kMachineNamePolicyVarName), "machine");
  }

  return result;
}

}  // namespace path_parser

}  // namespace policy
