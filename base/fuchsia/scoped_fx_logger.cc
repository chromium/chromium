// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/scoped_fx_logger.h"

#include <lib/fdio/directory.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/process/process.h"
#include "base/strings/string_piece.h"
#include "base/threading/platform_thread.h"

namespace base {

ScopedFxLogger::ScopedFxLogger() = default;
ScopedFxLogger::~ScopedFxLogger() = default;

// static
ScopedFxLogger ScopedFxLogger::CreateForProcessWithTag(base::StringPiece tag) {
  // CHECK()ing or LOG()ing inside this function is safe, since it is only
  // called to initialize logging, not during individual logging operations.

  fuchsia::logger::LogSinkHandle log_sink;
  zx_status_t status =
      fdio_service_connect("/svc/fuchsia.logger.LogSink",
                           log_sink.NewRequest().TakeChannel().release());
  if (status != ZX_OK) {
    ZX_LOG(ERROR, status) << "connect(LogSink) failed";
    return {};
  }

  return CreateFromLogSinkWithTag(std::move(log_sink), tag);
}

// static
ScopedFxLogger ScopedFxLogger::CreateFromLogSink(
    fuchsia::logger::LogSinkHandle log_sink_handle) {
  return CreateFromLogSinkWithTag(std::move(log_sink_handle), {});
}

// static
ScopedFxLogger ScopedFxLogger::CreateFromLogSinkWithTag(
    fuchsia::logger::LogSinkHandle log_sink_handle,
    base::StringPiece tag) {
  // CHECK()ing or LOG()ing inside this function is safe, since it is only
  // called to initialize logging, not during individual logging operations.

  // Attempts to create a kernel socket object should never fail.
  zx::socket local, remote;
  zx_status_t status = zx::socket::create(ZX_SOCKET_DATAGRAM, &local, &remote);
  ZX_CHECK(status == ZX_OK, status) << "zx_socket_create() failed";

  // ConnectStructured() may fail if e.g. the LogSink has disconnected already.
  fuchsia::logger::LogSinkSyncPtr log_sink;
  log_sink.Bind(std::move(log_sink_handle));
  status = log_sink->ConnectStructured(std::move(remote));
  if (status != ZX_OK) {
    ZX_LOG(ERROR, status) << "ConnectStructured() failed";
    return {};
  }

  return ScopedFxLogger(tag, std::move(local));
}

void ScopedFxLogger::LogMessage(base::StringPiece file,
                                uint32_t line_number,
                                base::StringPiece msg,
                                FuchsiaLogSeverity severity) {
  if (!socket_.is_valid())
    return;

  // It is not safe to use e.g. CHECK() or LOG() macros here, since those
  // may result in reentrancy if this instance is used for routing process-
  // global logs to the system logger.

  fuchsia_syslog::LogBuffer buffer;
  buffer.BeginRecord(
      severity, cpp17::string_view(file.data(), file.size()), line_number,
      cpp17::string_view(msg.data(), msg.size()), {}, false, socket_.borrow(),
      0, base::Process::Current().Pid(), base::PlatformThread::CurrentId());
  if (!tag_.empty()) {
    buffer.WriteKeyValue("tag", tag_);
  }
  buffer.FlushRecord();
}

ScopedFxLogger::ScopedFxLogger(base::StringPiece tag, zx::socket socket)
    : tag_(tag), socket_(std::move(socket)) {}

}  // namespace base
