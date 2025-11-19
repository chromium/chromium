// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/scoped_fx_logger.h"

#include <lib/component/incoming/cpp/protocol.h>
#include <lib/fdio/directory.h>
#include <stdio.h>

#include <string_view>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/fuchsia/fuchsia_component_connect.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/process/process.h"
#include "base/threading/platform_thread.h"

namespace base {

namespace {

inline FuchsiaLogSeverity LogSeverityToFuchsiaLogSeverity(
    logging::LogSeverity severity) {
  switch (severity) {
    case logging::LOGGING_INFO:
      return FUCHSIA_LOG_INFO;
    case logging::LOGGING_WARNING:
      return FUCHSIA_LOG_WARNING;
    case logging::LOGGING_ERROR:
      return FUCHSIA_LOG_ERROR;
    case logging::LOGGING_FATAL:
      // Don't use FX_LOG_FATAL, otherwise fx_logger_log() will abort().
      return FUCHSIA_LOG_ERROR;
  }
  if (severity > -3) {
    // LOGGING_VERBOSE levels 1 and 2.
    return FUCHSIA_LOG_DEBUG;
  }
  // LOGGING_VERBOSE levels 3 and higher, or incorrect levels.
  return FUCHSIA_LOG_TRACE;
}

}  // namespace

ScopedFxLogger::ScopedFxLogger() = default;
ScopedFxLogger::~ScopedFxLogger() = default;

ScopedFxLogger::ScopedFxLogger(ScopedFxLogger&& other) = default;
ScopedFxLogger& ScopedFxLogger::operator=(ScopedFxLogger&& other) = default;

// static
ScopedFxLogger ScopedFxLogger::CreateForProcess(
    std::vector<std::string_view> tags) {
  // CHECK()ing or LOG()ing inside this function is safe, since it is only
  // called to initialize logging, not during individual logging operations.

  auto log_sink_client_end = component::Connect<fuchsia_logger::LogSink>();
  if (log_sink_client_end.is_error()) {
    LOG(ERROR) << FidlConnectionErrorMessage(log_sink_client_end);
    return {};
  }

  // Rather than relying on automatic LogSink attribution via COMPONENT_NAME,
  // prepend a tag based on the calling process' name.  COMPONENT_NAME may be
  // mis-attributed, in some Component configurations, to a parent or caller
  // component, from which the process' LogSink service is routed.
  std::string program_name = base::CommandLine::ForCurrentProcess()
                                 ->GetProgram()
                                 .BaseName()
                                 .AsUTF8Unsafe();
  tags.insert(tags.begin(), program_name);

  return CreateFromLogSink(std::move(log_sink_client_end.value()),
                           std::move(tags));
}

// static
ScopedFxLogger ScopedFxLogger::CreateFromLogSink(
    fidl::ClientEnd<fuchsia_logger::LogSink> log_sink_client_end,
    std::vector<std::string_view> tags) {
  // CHECK()ing or LOG()ing inside this function is safe, since it is only
  // called to initialize logging, not during individual logging operations.

  // TODO(zijiehe): Update the signature to accept std::vector<std::string> and
  // remove this conversion.
  std::vector<std::string> str_tags;
  for (std::string_view tag : tags) {
    str_tags.push_back(std::string(tag));
  }
  std::vector<const char*> c_str_tags;
  for (const std::string& tag : str_tags) {
    c_str_tags.push_back(tag.c_str());
  }
  if (zx::result<fuchsia_logging::Logger> logger =
          fuchsia_logging::Logger::Create(fuchsia_logging::RawLogSettings{
              .log_sink = log_sink_client_end.TakeChannel().release(),
              .tags = c_str_tags.data(),
              .tags_count = c_str_tags.size(),
          });
      logger.is_error()) {
    ZX_LOG(ERROR, logger.status_value()) << "Unable to create Fuchsia logger";
    return {};
  } else {
    return ScopedFxLogger(*std::move(logger));
  }
}

void ScopedFxLogger::LogMessage(std::string_view file,
                                uint32_t line_number,
                                std::string_view msg,
                                logging::LogSeverity severity) {
  if (!logger_.IsValid()) {
    return;
  }

  FuchsiaLogSeverity fuchsia_severity =
      LogSeverityToFuchsiaLogSeverity(severity);

  // It is not safe to use e.g. CHECK() or LOG() macros here, since those
  // may result in reentrancy if this instance is used for routing process-
  // global logs to the system logger.

  fuchsia_logging::LogBuffer buffer;
  buffer.BeginRecord(
      fuchsia_severity, std::string_view(file.data(), file.size()), line_number,
      std::string_view(msg.data(), msg.size()), 0,
      base::Process::Current().Pid(), base::PlatformThread::CurrentId().raw());
  if (logger_.FlushBuffer(buffer).is_error()) {
    fprintf(stderr, "Logger::FlushBuffer failed\n");
  }
}

}  // namespace base
