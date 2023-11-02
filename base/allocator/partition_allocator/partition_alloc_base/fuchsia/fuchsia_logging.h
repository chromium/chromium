// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_BASE_FUCHSIA_FUCHSIA_LOGGING_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_BASE_FUCHSIA_FUCHSIA_LOGGING_H_

#include <lib/fit/function.h>
#include <zircon/types.h>

#include "base/allocator/partition_allocator/partition_alloc_base/component_export.h"
#include "base/allocator/partition_allocator/partition_alloc_base/debug/debugging_buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_base/logging.h"
#include "build/build_config.h"

// Use the PA_ZX_LOG family of macros along with a zx_status_t containing a
// Zircon error. The error value will be decoded so that logged messages explain
// the error.

namespace partition_alloc::internal::logging {

class PA_COMPONENT_EXPORT(PARTITION_ALLOC) ZxLogMessage
    : public logging::LogMessage {
 public:
  ZxLogMessage(const char* file_path,
               int line,
               LogSeverity severity,
               zx_status_t zx_err);

  ZxLogMessage(const ZxLogMessage&) = delete;
  ZxLogMessage& operator=(const ZxLogMessage&) = delete;

  ~ZxLogMessage() override;

 private:
  zx_status_t zx_err_;
};

}  // namespace partition_alloc::internal::logging

#define PA_ZX_LOG_STREAM(severity, zx_err) \
  PA_COMPACT_GOOGLE_LOG_EX_##severity(ZxLogMessage, zx_err).stream()

#define PA_ZX_LOG(severity, zx_err) \
  PA_LAZY_STREAM(PA_ZX_LOG_STREAM(severity, zx_err), PA_LOG_IS_ON(severity))
#define PA_ZX_LOG_IF(severity, condition, zx_err)    \
  PA_LAZY_STREAM(PA_ZX_LOG_STREAM(severity, zx_err), \
                 PA_LOG_IS_ON(severity) && (condition))

#define PA_ZX_CHECK(condition, zx_err)                          \
  PA_LAZY_STREAM(PA_ZX_LOG_STREAM(FATAL, zx_err), !(condition)) \
      << "Check failed: " #condition << ". "

#define PA_ZX_DLOG(severity, zx_err) \
  PA_LAZY_STREAM(PA_ZX_LOG_STREAM(severity, zx_err), PA_DLOG_IS_ON(severity))

#if BUILDFLAG(PA_DCHECK_IS_ON)
#define PA_ZX_DLOG_IF(severity, condition, zx_err)   \
  PA_LAZY_STREAM(PA_ZX_LOG_STREAM(severity, zx_err), \
                 PA_DLOG_IS_ON(severity) && (condition))
#else  // BUILDFLAG(PA_DCHECK_IS_ON)
#define PA_ZX_DLOG_IF(severity, condition, zx_err) PA_EAT_STREAM_PARAMETERS
#endif  // BUILDFLAG(PA_DCHECK_IS_ON)

#define PA_ZX_DCHECK(condition, zx_err)                      \
  PA_LAZY_STREAM(PA_ZX_LOG_STREAM(DCHECK, zx_err),           \
                 BUILDFLAG(PA_DCHECK_IS_ON) && !(condition)) \
      << "Check failed: " #condition << ". "

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_BASE_FUCHSIA_FUCHSIA_LOGGING_H_
