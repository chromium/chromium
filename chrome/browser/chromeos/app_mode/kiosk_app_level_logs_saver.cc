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
    const KioskAppLevelLogsSaver::KioskLogMessage& log) {
  std::u16string formatted_message;
  if (log.source().has_value() && log.line_no().has_value()) {
    formatted_message = l10n_util::FormatString(
        u"$1:$2 \"$3\"",
        {log.source().value(), base::NumberToString16(log.line_no().value()),
         log.message()},
        nullptr);
  } else {
    formatted_message = log.message();
  }

  if (!log.untrusted_stack_trace().has_value()) {
    return formatted_message;
  }

  return l10n_util::FormatString(
      u"$1\nstack_trace: $2",
      {formatted_message, log.untrusted_stack_trace().value()}, nullptr);
}

}  // namespace

KioskAppLevelLogsSaver::KioskLogMessage::KioskLogMessage(
    const std::u16string& message,
    blink::mojom::ConsoleMessageLevel severity)
    : message_(message), severity_(severity) {}

KioskAppLevelLogsSaver::KioskLogMessage::KioskLogMessage(
    const std::u16string& message,
    blink::mojom::ConsoleMessageLevel severity,
    int line_no,
    std::u16string source,
    std::optional<std::u16string> untrusted_stack_trace)
    : message_(message),
      severity_(severity),
      line_no_(line_no),
      source_(source),
      untrusted_stack_trace_(untrusted_stack_trace) {}

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
  logger_callback_.Run(FormatLogMessage(log), log.severity());
}

}  // namespace chromeos
