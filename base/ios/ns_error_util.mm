// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ios/ns_error_util.h"

#import <Foundation/Foundation.h>

#include "base/check.h"

namespace base::ios {

namespace {
// Iterates through |error|'s underlying errors and returns them in an array.
NSArray<NSError*>* GetFullErrorChainForError(NSError* error) {
  NSMutableArray<NSError*>* error_chain = [NSMutableArray array];
  NSError* current_error = error;
  while (current_error) {
    DCHECK([current_error isKindOfClass:[NSError class]]);
    [error_chain addObject:current_error];
    current_error = current_error.userInfo[NSUnderlyingErrorKey];
  }
  return error_chain;
}
}  // namespace

NSError* GetFinalUnderlyingErrorFromError(NSError* error) {
  DCHECK(error);
  return GetFullErrorChainForError(error).lastObject;
}

NSError* ErrorWithAppendedUnderlyingError(NSError* original_error,
                                          NSError* underlying_error) {
  DCHECK(original_error);
  DCHECK(underlying_error);
  NSArray<NSError*>* error_chain = GetFullErrorChainForError(original_error);
  NSError* current_error = underlying_error;
  for (size_t idx = error_chain.count; idx > 0; --idx) {
    NSError* error = error_chain[idx - 1];
    NSMutableDictionary* user_info = [error.userInfo mutableCopy];
    user_info[NSUnderlyingErrorKey] = current_error;
    current_error = [NSError errorWithDomain:error.domain
                                        code:error.code
                                    userInfo:user_info];
  }
  return current_error;
}

}  // namespace base::ios
