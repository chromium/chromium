// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_path_parser.h"

#import <Cocoa/Cocoa.h>
#include <stddef.h>
#import <SystemConfiguration/SCDynamicStore.h>
#import <SystemConfiguration/SCDynamicStoreCopySpecific.h>
#import <SystemConfiguration/SystemConfiguration.h>

#include <string>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/macros.h"
#include "base/strings/sys_string_conversions.h"
#include "build/branding_buildflags.h"
#include "components/policy/policy_constants.h"

namespace policy {

namespace path_parser {

const char kUserNamePolicyVarName[] = "${user_name}";
const char kMachineNamePolicyVarName[] = "${machine_name}";
const char kMacUsersDirectory[] = "${users}";
const char kMacDocumentsFolderVarName[] = "${documents}";

struct MacFolderNamesToSPDMapping {
  const char* name;
  NSSearchPathDirectory id;
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
      NSArray* searchpaths = NSSearchPathForDirectoriesInDomains(
          mapping.id, NSAllDomainsMask, true);
      if ([searchpaths count] > 0) {
        NSString *variable_value = [searchpaths objectAtIndex:0];
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
    base::ScopedCFTypeRef<SCDynamicStoreRef> store(SCDynamicStoreCreate(
        kCFAllocatorDefault, CFSTR("policy_subsystem"), nullptr, &context));
    base::ScopedCFTypeRef<CFStringRef> machinename(
        SCDynamicStoreCopyLocalHostName(store));
    if (machinename) {
      result.replace(position, strlen(kMachineNamePolicyVarName),
                     base::SysCFStringRefToUTF8(machinename));
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
  base::ScopedCFTypeRef<CFStringRef> bundle_id(
      base::SysUTF8ToCFStringRef(base::mac::BaseBundleID()));
#endif

  base::ScopedCFTypeRef<CFStringRef> key(
      base::SysUTF8ToCFStringRef(policy::key::kUserDataDir));
  base::ScopedCFTypeRef<CFPropertyListRef> value(
      CFPreferencesCopyAppValue(key, bundle_id));

  if (!value || !CFPreferencesAppValueIsForced(key, bundle_id))
    return;
  CFStringRef value_string = base::mac::CFCast<CFStringRef>(value);
  if (!value_string)
    return;

  // Now replace any vars the user might have used.
  std::string string_value = base::SysCFStringRefToUTF8(value_string);
  string_value = policy::path_parser::ExpandPathVariables(string_value);
  *user_data_dir = base::FilePath(string_value);
}

}  // namespace path_parser

}  // namespace policy
