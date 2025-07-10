// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_LEVEL_LOGS_SAVER_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_LEVEL_LOGS_SAVER_H_

#include <cstddef>
#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-data-view.h"

namespace chromeos {

class KioskAppLevelLogsSaver {
 public:
  using LoggerCallback =
      base::RepeatingCallback<void(const std::u16string& message,
                                   blink::mojom::ConsoleMessageLevel severity)>;

  KioskAppLevelLogsSaver();

  // This constructor accepts a logger callback to facilitate testing.
  explicit KioskAppLevelLogsSaver(LoggerCallback logger);

  KioskAppLevelLogsSaver(const KioskAppLevelLogsSaver&) = delete;
  KioskAppLevelLogsSaver& operator=(const KioskAppLevelLogsSaver&) = delete;
  ~KioskAppLevelLogsSaver();

  class KioskLogMessage {
   public:
    KioskLogMessage(const std::u16string& message,
                    blink::mojom::ConsoleMessageLevel severity);
    KioskLogMessage(const std::u16string& message,
                    blink::mojom::ConsoleMessageLevel severity,
                    int line_no,
                    std::u16string source,
                    std::optional<std::u16string> untrusted_stack_trace);

    KioskLogMessage(const KioskLogMessage&);
    KioskLogMessage(KioskLogMessage&&);
    KioskLogMessage& operator=(const KioskLogMessage&) = delete;
    KioskLogMessage& operator=(const KioskLogMessage&&) = delete;

    ~KioskLogMessage();

    const std::u16string& message() const { return message_; }

    blink::mojom::ConsoleMessageLevel severity() const { return severity_; }

    const std::optional<int>& line_no() const { return line_no_; }

    const std::optional<std::u16string>& source() const { return source_; }

    const std::optional<std::u16string>& untrusted_stack_trace() const {
      return untrusted_stack_trace_;
    }

   private:
    const std::u16string message_;
    const blink::mojom::ConsoleMessageLevel severity_;
    const std::optional<int> line_no_;
    const std::optional<std::u16string> source_;
    const std::optional<std::u16string> untrusted_stack_trace_;
  };

  void SaveLog(const KioskLogMessage& log);

 private:
  // Counts total saved logs entries. It is always not exceeding
  // `kMaximumLogEntriesToBeSaved` since extra logs are discarded.
  size_t total_logs_saved_ = 0;

  LoggerCallback logger_callback_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_LEVEL_LOGS_SAVER_H_
