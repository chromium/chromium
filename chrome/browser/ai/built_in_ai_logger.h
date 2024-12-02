// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_BUILT_IN_AI_LOGGER_H_
#define CHROME_BROWSER_AI_BUILT_IN_AI_LOGGER_H_

#include <string>

#include "base/logging.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "components/optimization_guide/core/optimization_guide_common.mojom-shared.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"

// Wrapper that logs a stream to OptimizationGuideLogger during destruction.
class AiLogMessage {
 public:
  AiLogMessage(const char* file, int line);
  std::ostream& stream() { return stream_; }
  inline ~AiLogMessage() { opt_guide_message_builder_ << stream_.str(); }

 private:
  std::ostringstream stream_;
  OptimizationGuideLogger::LogMessageBuilder opt_guide_message_builder_;
};

// Log stream that logs to the optimization guide log
// (chrome://on-device-internals). The log only occurs when optimization guide
// logging is enabled. Otherwise, the stream is never evaluated nor logged. This
// log stream is intended for use by code layers connecting the optimization
// guide and higher level built-in AI features.
#define BUILT_IN_AI_LOGGER()                     \
  LAZY_STREAM(                                   \
      AiLogMessage(__FILE__, __LINE__).stream(), \
      OptimizationGuideLogger::GetInstance() &&  \
          OptimizationGuideLogger::GetInstance()->ShouldEnableDebugLogs())

#endif  // CHROME_BROWSER_AI_BUILT_IN_AI_LOGGER_H_
