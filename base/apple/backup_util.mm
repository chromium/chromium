// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/apple/backup_util.h"

#import <Foundation/Foundation.h>

#include "base/apple/foundation_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"

namespace base::apple {

bool GetBackupExclusion(const FilePath& file_path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  NSURL* file_url = apple::FilePathToNSURL(file_path);
  DCHECK([file_url checkPromisedItemIsReachableAndReturnError:nil]);

  NSError* error = nil;
  NSNumber* value = nil;
  BOOL success = [file_url getResourceValue:&value
                                     forKey:NSURLIsExcludedFromBackupKey
                                      error:&error];
  if (!success) {
    LOG(ERROR) << base::SysNSStringToUTF8(error.description);
    return false;
  }

  return value && value.boolValue;
}

namespace {

bool SetBackupState(const FilePath& file_path, bool excluded) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  NSURL* file_url = apple::FilePathToNSURL(file_path);
  DCHECK([file_url checkPromisedItemIsReachableAndReturnError:nil]);

  NSError* error = nil;
  BOOL success = [file_url setResourceValue:@(excluded)
                                     forKey:NSURLIsExcludedFromBackupKey
                                      error:&error];
  LOG_IF(WARNING, !success) << base::SysNSStringToUTF8(error.description);
  return success;
}

}  // namespace

bool SetBackupExclusion(const FilePath& file_path) {
  return SetBackupState(file_path, true);
}

bool ClearBackupExclusion(const FilePath& file_path) {
  return SetBackupState(file_path, false);
}

}  // namespace base::apple
