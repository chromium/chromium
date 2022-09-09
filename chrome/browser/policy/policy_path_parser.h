// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_POLICY_PATH_PARSER_H_
#define CHROME_BROWSER_POLICY_POLICY_PATH_PARSER_H_

#include "base/files/file_path.h"
#include "build/build_config.h"

namespace policy {

namespace path_parser {

// This function is used to expand the variables in policy strings that
// represent paths. The set of supported variables differs between platforms
// but generally covers most standard locations that might be needed in the
// existing used cases.
// All platforms:
//   ${user_name}       - The user that is running Chrome (respects suids).
//                        (example : "johndoe")
//   ${machine_name}    - The machine name possibly with domain (example :
//                        "johnny.cg1.cooldomain.org")
// Windows only:
//   ${documents}       - The "Documents" folder for the current user.
//                        (example : "C:\Users\Administrator\Documents")
//   ${local_app_data}  - The Application Data folder for the current user.
//                        (example : "C:\Users\Administrator\AppData\Local")
//   ${roaming_app_data}- The Roamed AppData folder for the current user.
//                        (example : "C:\Users\Administrator\AppData\Roaming")
//   ${profile}         - The home folder for the current user.
//                        (example : "C:\Users\Administrator")
//   ${global_app_data} - The system-wide Application Data folder.
//                        (example : "C:\Users\All Users\AppData")
//   ${program_files}   - The "Program Files" folder for the current process.
//                        Depends on whether it is 32 or 64 bit process.
//                        (example : "C:\Program Files (x86)")
//   ${windows}         - The Windows folder
//                        (example : "C:\WINNT" or "C:\Windows")
//   ${client_name}     - The name of the client as reported by the WTS system.
//                        (example : "clientone")
//   ${session_name}    - The name of the session as reported by the WTS system.
//                        (example : "WinSta0", "RDP-Tcp#1")
// MacOS only:
//   ${users}           - The folder where users profiles are stored
//                        (example : "/Users")
//   ${documents}       - The "Documents" folder of the current user.
//                        (example : "/Users/johndoe/Documents")
// Any non recognized variable is not being translated at all. Variables are
// translated only once in every string because for most of these there is no
// sense in concatenating them more than once in a single path.
base::FilePath::StringType ExpandPathVariables(
    const base::FilePath::StringType& untranslated_string);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
// A helper function used to read the UserDataDir path policy without relying on
// any policy infrastructure. This is required because this policy is needed
// much earlier before the PrefService is initialized.
// The function will fill |user_data_dir| if the policy "UserDataDir" is set and
// leave it intact if the policy is missing. If the policy is set it should
// override any manual changes to the profile path the user might have made so
// this function should be used to verify no policy is specified whenever the
// profile path is not read from the PathService which already takes this into
// account.
void CheckUserDataDirPolicy(base::FilePath* user_data_dir);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

}  // namespace path_parser

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_POLICY_PATH_PARSER_H_
