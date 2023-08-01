// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/mac_logging.h"

#import <Foundation/Foundation.h>

#include <iomanip>

#include "build/build_config.h"

#if !BUILDFLAG(IS_IOS)
#include <CoreServices/CoreServices.h>
#endif

namespace logging {

std::string DescriptionFromOSStatus(OSStatus err) {
  NSError* error =
      [NSError errorWithDomain:NSOSStatusErrorDomain code:err userInfo:nil];
  return error.description.UTF8String;
}

OSStatusLogMessage::OSStatusLogMessage(const char* file_path,
                                       int line,
                                       LogSeverity severity,
                                       OSStatus status)
    : LogMessage(file_path, line, severity),
      status_(status) {
}

OSStatusLogMessage::~OSStatusLogMessage() {
#if BUILDFLAG(IS_IOS)
  // TODO(crbug.com/546375): Consider using NSError with NSOSStatusErrorDomain
  // to try to get a description of the failure.
  stream() << ": " << status_;
#else
  stream() << ": "
           << DescriptionFromOSStatus(status_)
           << " ("
           << status_
           << ")";
#endif
}

}  // namespace logging
