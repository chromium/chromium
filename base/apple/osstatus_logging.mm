// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/apple/osstatus_logging.h"

#import <Foundation/Foundation.h>

#include <iomanip>

namespace logging {

std::string DescriptionFromOSStatus(OSStatus err) {
  NSError* error = [NSError errorWithDomain:NSOSStatusErrorDomain
                                       code:err
                                   userInfo:nil];
  return error.description.UTF8String;
}

OSStatusLogMessage::OSStatusLogMessage(const char* file_path,
                                       int line,
                                       LogSeverity severity,
                                       OSStatus status)
    : LogMessage(file_path, line, severity), status_(status) {}

OSStatusLogMessage::~OSStatusLogMessage() {
  stream() << ": " << DescriptionFromOSStatus(status_) << " (" << status_
           << ")";
}

}  // namespace logging
