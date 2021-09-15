// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FUCHSIA_FUCHSIA_LOGGING_H_
#define BASE_FUCHSIA_FUCHSIA_LOGGING_H_

#include <zircon/types.h>

#include "base/base_export.h"
#include "base/logging.h"
#include "base/macros.h"
#include "build/build_config.h"

// Use the ZX_LOG family of macros along with a zx_status_t containing a Zircon
// error. The error value will be decoded so that logged messages explain the
// error.

namespace logging {

class BASE_EXPORT ZxLogMessage : public logging::LogMessage {
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

}  // namespace logging

#define ZX_LOG_STREAM(severity, zx_err) \
  COMPACT_GOOGLE_LOG_EX_##severity(ZxLogMessage, zx_err).stream()

#define ZX_LOG(severity, zx_err) \
  LAZY_STREAM(ZX_LOG_STREAM(severity, zx_err), LOG_IS_ON(severity))
#define ZX_LOG_IF(severity, condition, zx_err) \
  LAZY_STREAM(ZX_LOG_STREAM(severity, zx_err), \
              LOG_IS_ON(severity) && (condition))

#define ZX_CHECK(condition, zx_err)                       \
  LAZY_STREAM(ZX_LOG_STREAM(FATAL, zx_err), !(condition)) \
      << "Check failed: " #condition << ". "

#define ZX_DLOG(severity, zx_err) \
  LAZY_STREAM(ZX_LOG_STREAM(severity, zx_err), DLOG_IS_ON(severity))

#if DCHECK_IS_ON()
#define ZX_DLOG_IF(severity, condition, zx_err) \
  LAZY_STREAM(ZX_LOG_STREAM(severity, zx_err),  \
              DLOG_IS_ON(severity) && (condition))
#else  // DCHECK_IS_ON()
#define ZX_DLOG_IF(severity, condition, zx_err) EAT_STREAM_PARAMETERS
#endif  // DCHECK_IS_ON()

#define ZX_DCHECK(condition, zx_err)                                         \
  LAZY_STREAM(ZX_LOG_STREAM(DCHECK, zx_err), DCHECK_IS_ON() && !(condition)) \
      << "Check failed: " #condition << ". "

#endif  // BASE_FUCHSIA_FUCHSIA_LOGGING_H_
