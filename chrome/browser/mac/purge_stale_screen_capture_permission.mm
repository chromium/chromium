// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mac/purge_stale_screen_capture_permission.h"

#include <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>
#include <Security/Security.h>

#include <string>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_logging.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "build/branding_buildflags.h"
#include "sql/database.h"
#include "sql/statement.h"

namespace chrome {

namespace {

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)

bool RequirementForBundleIDNeedsReset(NSString* bundle_id) {
  base::FilePath local_application_support_path;
  if (!base::mac::GetLocalDirectory(NSApplicationSupportDirectory,
                                    &local_application_support_path)) {
    return true;
  }
  base::FilePath local_tcc_db_path(
      local_application_support_path.Append("com.apple.TCC").Append("TCC.db"));
  sql::DatabaseOptions options;
  options.exclusive_locking = false;
  sql::Database tcc_db = sql::Database(options);

  if (!tcc_db.Open(local_tcc_db_path)) {
    // On macOS 10.15 and macOS 11 this open is expected to fail due to SIP.
    return true;
  }
  sql::Statement s(tcc_db.GetUniqueStatement(
      "SELECT csreq FROM access WHERE client_type=0 AND client=? AND "
      "service='kTCCServiceScreenCapture'"));
  s.BindString(0, bundle_id.UTF8String);

  while (s.Step()) {
    base::span<const uint8_t> csreq_blob = s.ColumnBlob(0);
    if (csreq_blob.empty()) {
      return true;
    }

    base::ScopedCFTypeRef<CFDataRef> csreq_data(
        CFDataCreate(nullptr, csreq_blob.data(), csreq_blob.size()));
    base::ScopedCFTypeRef<SecRequirementRef> requirement;
    OSStatus status = SecRequirementCreateWithData(
        csreq_data, kSecCSDefaultFlags, requirement.InitializeInto());
    if (status != errSecSuccess) {
      OSSTATUS_LOG(ERROR, status) << "SecRequirementCreateWithData";
      return true;
    }
    base::ScopedCFTypeRef<CFStringRef> requirement_string;
    status = SecRequirementCopyString(requirement, kSecCSDefaultFlags,
                                      requirement_string.InitializeInto());
    if (status != errSecSuccess) {
      OSSTATUS_LOG(ERROR, status) << "SecRequirementCopyString";
      return true;
    }

    static constexpr char kCurrentRequirementTail[] =
        " and certificate leaf[subject.OU] = EQHXZ8M8AV";
    if (!base::EndsWith(base::SysCFStringRefToUTF8(requirement_string),
                        kCurrentRequirementTail)) {
      return true;
    }
  }

  return !s.Succeeded();
}

bool ResetTCCScreenCaptureForBundleID(NSString* bundle_id) {
  if (!bundle_id.length) {
    return false;
  }
  std::vector<std::string> argv = {"/usr/bin/tccutil", "reset", "ScreenCapture",
                                   bundle_id.UTF8String};

  base::LaunchOptions launch_options;
  base::Process p = base::LaunchProcess(argv, launch_options);
  int status;
  return p.WaitForExit(&status) && status == 0;
}

bool AttemptPurgeStaleScreenCapturePermission() {
  NSString* bundle_identifier = base::mac::MainBundle().bundleIdentifier;
  if (RequirementForBundleIDNeedsReset(bundle_identifier)) {
    // Paranoia about the exec failing for some reason. Retry once for good
    // measure.
    for (int i = 0; i < 2; ++i) {
      if (ResetTCCScreenCaptureForBundleID(bundle_identifier)) {
        // The stale record has been purged.
        return true;
      }
    }
    return false;
  }

  // The requirement is valid or doesn't exist.
  return true;
}

#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace

void PurgeStaleScreenCapturePermission() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)

  // TCC doesn’t supervise screen capture until macOS 10.15.
  if (@available(macOS 10.15, *)) {
    static NSString* const kPreferenceKeyBase =
        @"PurgeStaleScreenCapturePermissionSuccessEarly2022";
    static NSString* const kPreferenceKeyAttemptsSuffix = @"Attempts";
    static NSString* const kPreferenceKeySuccessSuffix = @"Success";
    NSString* success_key = [kPreferenceKeyBase
        stringByAppendingString:kPreferenceKeySuccessSuffix];
    NSString* attempts_key = [kPreferenceKeyBase
        stringByAppendingString:kPreferenceKeyAttemptsSuffix];

    NSUserDefaults* user_defaults = [NSUserDefaults standardUserDefaults];
    if ([user_defaults boolForKey:success_key]) {
      return;
    }

    const int kAttemptsMax = 3;
    int attempts_value = [user_defaults integerForKey:attempts_key];
    if ((attempts_value >= kAttemptsMax)) {
      return;
    }
    [user_defaults setInteger:++attempts_value forKey:attempts_key];
    base::ScopedAllowBlocking allow_blocking;
    if (AttemptPurgeStaleScreenCapturePermission()) {
      // Future startups will now return from PurgeStaleScreenCapturePermission
      // early.
      [user_defaults setBool:YES forKey:success_key];
    }
  }

#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

}  // namespace chrome
