// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/backup_util.h"

#include <CoreServices/CoreServices.h>
#import <Foundation/Foundation.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_logging.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"

namespace base::mac {

bool GetBackupExclusion(const FilePath& file_path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

#if BUILDFLAG(IS_MAC)
  return CSBackupIsItemExcluded(FilePathToCFURL(file_path), nullptr);
#elif BUILDFLAG(IS_IOS)
  NSURL* file_url = FilePathToNSURL(file_path);
  DCHECK([[NSFileManager defaultManager] fileExistsAtPath:file_url.path]);

  NSError* error = nil;
  NSNumber* value = nil;
  BOOL success = [file_url getResourceValue:&value
                                     forKey:NSURLIsExcludedFromBackupKey
                                      error:&error];
  if (!success) {
    LOG(ERROR) << base::SysNSStringToUTF8([error description]);
    return false;
  }

  return value && value.boolValue;
#endif
}

namespace {

bool SetBackupState(const FilePath& file_path, bool excluded) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

#if BUILDFLAG(IS_MAC)
  // When excludeByPath is true the application must be running with root
  // privileges (admin for 10.6 and earlier) but the URL does not have to
  // already exist. When excludeByPath is false the URL must already exist but
  // can be used in non-root (or admin as above) mode. We use false so that
  // non-root (or admin) users don't get their TimeMachine drive filled up with
  // unnecessary backups.
  OSStatus os_err =
      CSBackupSetItemExcluded(FilePathToCFURL(file_path), excluded,
                              /*excludeByPath=*/FALSE);
  OSSTATUS_DLOG_IF(WARNING, os_err != noErr, os_err)
      << "Failed to set backup exclusion for file '"
      << file_path.value().c_str() << "'";
  return os_err == noErr;
#elif BUILDFLAG(IS_IOS)
  NSURL* file_url = FilePathToNSURL(file_path);
  DCHECK([[NSFileManager defaultManager] fileExistsAtPath:file_url.path]);

  NSError* error = nil;
  BOOL success = [file_url setResourceValue:@(excluded)
                                     forKey:NSURLIsExcludedFromBackupKey
                                      error:&error];
  LOG_IF(WARNING, !success) << base::SysNSStringToUTF8([error description]);
  return success;
#endif
}

}  // namespace

bool SetBackupExclusion(const FilePath& file_path) {
  return SetBackupState(file_path, true);
}

bool ClearBackupExclusion(const FilePath& file_path) {
  return SetBackupState(file_path, false);
}

}  // namespace base::mac
