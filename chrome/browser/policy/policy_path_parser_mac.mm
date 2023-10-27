// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_path_parser.h"

#import <Cocoa/Cocoa.h>
#import <SystemConfiguration/SystemConfiguration.h>
#include <stddef.h>

#include <string>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "build/branding_buildflags.h"
#include "components/policy/policy_constants.h"

namespace policy::path_parser {

const char kUserNamePolicyVarName[] = "${user_name}";
const char kMachineNamePolicyVarName[] = "${machine_name}";
const char kMacUsersDirectory[] = "${users}";
const char kMacDocumentsFolderVarName[] = "${documents}";

struct MacFolderNamesToSPDMapping {
  const char* name;
  NSSearchPathDirectory search_path_directory;
};

// Mapping from variable names to MacOS NSSearchPathDirectory ids.
const MacFolderNamesToSPDMapping mac_folder_mapping[] = {
    {kMacUsersDirectory, NSUserDirectory},
    {kMacDocumentsFolderVarName, NSDocumentDirectory}};

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
  // First translate all path variables we recognize.
  for (const auto& mapping : mac_folder_mapping) {
    size_t position = result.find(mapping.name);
    if (position != std::string::npos) {
      NSArray<NSString*>* search_paths = NSSearchPathForDirectoriesInDomains(
          mapping.search_path_directory, NSAllDomainsMask, true);
      if (search_paths.count > 0) {
        NSString* variable_value = search_paths[0];
        result.replace(position, strlen(mapping.name),
                       base::SysNSStringToUTF8(variable_value));
      }
    }
  }
  // Next translate two special variables ${user_name} and ${machine_name}
  size_t position = result.find(kUserNamePolicyVarName);
  if (position != std::string::npos) {
    NSString* username = NSUserName();
    if (username) {
      result.replace(position, strlen(kUserNamePolicyVarName),
                     base::SysNSStringToUTF8(username));
    } else {
      LOG(ERROR) << "Username variable can not be resolved.";
    }
  }
  position = result.find(kMachineNamePolicyVarName);
  if (position != std::string::npos) {
    SCDynamicStoreContext context = {0, nullptr, nullptr, nullptr};
    base::apple::ScopedCFTypeRef<SCDynamicStoreRef> store(SCDynamicStoreCreate(
        kCFAllocatorDefault, CFSTR("policy_subsystem"), nullptr, &context));
    base::apple::ScopedCFTypeRef<CFStringRef> machine_name(
        SCDynamicStoreCopyLocalHostName(store.get()));
    if (machine_name) {
      result.replace(position, strlen(kMachineNamePolicyVarName),
                     base::SysCFStringRefToUTF8(machine_name.get()));
    } else {
      int error = SCError();
      LOG(ERROR) << "Machine name variable can not be resolved. Error: "
                 << error << " - " << SCErrorString(error);
      // Revert to a safe default if it is impossible to fetch the real name.
      result.replace(position, strlen(kMachineNamePolicyVarName), "localhost");
    }
  }
  return result;
}

void CheckUserDataDirPolicy(base::FilePath* user_data_dir) {
  // Since the configuration management infrastructure is not initialized when
  // this code runs, read the policy preference directly.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Explicitly access the "com.google.Chrome" bundle ID, no matter what this
  // app's bundle ID actually is. All channels of Chrome should obey the same
  // policies.
  CFStringRef bundle_id = CFSTR("com.google.Chrome");
#else
  base::apple::ScopedCFTypeRef<CFStringRef> bundle_id_scoper =
      base::SysUTF8ToCFStringRef(base::apple::BaseBundleID());
  CFStringRef bundle_id = bundle_id_scoper.get();
#endif

  base::apple::ScopedCFTypeRef<CFStringRef> key =
      base::SysUTF8ToCFStringRef(policy::key::kUserDataDir);
  base::apple::ScopedCFTypeRef<CFPropertyListRef> value(
      CFPreferencesCopyAppValue(key.get(), bundle_id));

  if (!value || !CFPreferencesAppValueIsForced(key.get(), bundle_id)) {
    return;
  }
  CFStringRef value_string = base::apple::CFCast<CFStringRef>(value.get());
  if (!value_string)
    return;

  // Now replace any vars the user might have used.
  std::string string_value = base::SysCFStringRefToUTF8(value_string);
  string_value = policy::path_parser::ExpandPathVariables(string_value);
  *user_data_dir = base::FilePath(string_value);
}

}  // namespace policy::path_parser
