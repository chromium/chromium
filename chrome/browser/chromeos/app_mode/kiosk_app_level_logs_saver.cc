// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_app_level_logs_saver.h"

#include <cstddef>
#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/syslog_logging.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-data-view.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace {

// Maximum number of log entries to save per session. Once this limit is
// reached, new logs will be discarded.
constexpr size_t kMaximumLogEntriesToBeSaved = 3000;

void SaveLogAccordingToSeverity(const std::u16string& message,
                                blink::mojom::ConsoleMessageLevel severity) {
  switch (severity) {
    case blink::mojom::ConsoleMessageLevel::kError:
      SYSLOG(ERROR) << message;
      break;
    case blink::mojom::ConsoleMessageLevel::kWarning:
      SYSLOG(WARNING) << message;
      break;
    case blink::mojom::ConsoleMessageLevel::kVerbose:
    case blink::mojom::ConsoleMessageLevel::kInfo:
    default:
      SYSLOG(INFO) << message;
  }
}

std::u16string FormatLogMessage(
    const std::u16string& source,
    const std::u16string& message,
    int line_no,
    const std::optional<std::u16string> stack_trace) {
  std::u16string formatted_message = l10n_util::FormatString(
      u"$1:$2 \"$3\"", {source, base::NumberToString16(line_no), message},
      nullptr);

  if (!stack_trace.has_value()) {
    return formatted_message;
  }

  return l10n_util::FormatString(u"$1\nstack_trace: $2",
                                 {formatted_message, stack_trace.value()},
                                 nullptr);
}

}  // namespace

KioskAppLevelLogsSaver::KioskLogMessage::KioskLogMessage(
    const std::u16string& message,
    blink::mojom::ConsoleMessageLevel severity,
    int line_no,
    const std::u16string& source,
    std::optional<std::u16string> untrusted_stack_trace)
    : message(message),
      severity(severity),
      line_no(line_no),
      source(source),
      untrusted_stack_trace(untrusted_stack_trace) {}

KioskAppLevelLogsSaver::KioskLogMessage::KioskLogMessage(
    const KioskLogMessage&) = default;

KioskAppLevelLogsSaver::KioskLogMessage::KioskLogMessage(KioskLogMessage&&) =
    default;

KioskAppLevelLogsSaver::KioskLogMessage::~KioskLogMessage() = default;

KioskAppLevelLogsSaver::KioskAppLevelLogsSaver()
    : KioskAppLevelLogsSaver(base::BindRepeating(&SaveLogAccordingToSeverity)) {
}

KioskAppLevelLogsSaver::KioskAppLevelLogsSaver(LoggerCallback logger_callback)
    : logger_callback_(std::move(logger_callback)) {}

KioskAppLevelLogsSaver::~KioskAppLevelLogsSaver() = default;

void KioskAppLevelLogsSaver::SaveLog(const KioskLogMessage& log) {
  if (total_logs_saved_ >= kMaximumLogEntriesToBeSaved) {
    return;
  }

  total_logs_saved_++;
  logger_callback_.Run(FormatLogMessage(log.source, log.message, log.line_no,
                                        log.untrusted_stack_trace),
                       log.severity);
}

}  // namespace chromeos
