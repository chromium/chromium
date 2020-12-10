// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/scoped_fx_logger.h"

#include "base/fuchsia/fuchsia_logging.h"

namespace base {

ScopedFxLogger CreateFxLoggerFromLogSink(
    fuchsia::logger::LogSinkHandle log_sink) {
  fx_logger_config_t config = {
      // Selecting based on log level is handled outside the fx_logger.
      .min_severity = FX_LOG_ALL,
      .console_fd = -1,
      .log_sink_channel = log_sink.TakeChannel().release(),
      // Do not set any custom tags.
      .tags = nullptr,
      .num_tags = 0,
  };

  fx_logger_t* fx_logger = nullptr;
  zx_status_t status = fx_logger_create(&config, &fx_logger);
  if (status != ZX_OK) {
    ZX_LOG(ERROR, status) << "fx_logger_create";
    return nullptr;
  }

  return ScopedFxLogger(fx_logger);
}

}  // namespace base
