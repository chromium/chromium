// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/partition_alloc_base/apple/mach_logging.h"

#include <iomanip>
#include <string>

#include "partition_alloc/partition_alloc_base/strings/stringprintf.h"

namespace {

std::string FormatMachErrorNumber(mach_error_t mach_err) {
  // For the os/kern subsystem, give the error number in decimal as in
  // <mach/kern_return.h>. Otherwise, give it in hexadecimal to make it easier
  // to visualize the various bits. See <mach/error.h>.
  if (mach_err >= 0 && mach_err < KERN_RETURN_MAX) {
    return partition_alloc::internal::base::TruncatingStringPrintf(" (%d)",
                                                                   mach_err);
  }
  return partition_alloc::internal::base::TruncatingStringPrintf(" (0x%08x)",
                                                                 mach_err);
}

}  // namespace

namespace partition_alloc::internal::logging {

MachLogMessage::MachLogMessage(const char* file_path,
                               int line,
                               LogSeverity severity,
                               mach_error_t mach_err)
    : LogMessage(file_path, line, severity), mach_err_(mach_err) {}

MachLogMessage::~MachLogMessage() {
  stream() << ": " << mach_error_string(mach_err_)
           << FormatMachErrorNumber(mach_err_).c_str();
}

}  // namespace partition_alloc::internal::logging
