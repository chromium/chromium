// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_web_contents_observer.h"

#include <cstdint>
#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_level_logs_saver.h"
#include "content/public/browser/web_contents_observer.h"

namespace chromeos {

KioskWebContentsObserver::KioskWebContentsObserver(
    content::WebContents* web_contents,
    LoggerCallback logger_callback)
    : content::WebContentsObserver(web_contents),
      logger_callback_(std::move(logger_callback)) {}

KioskWebContentsObserver::~KioskWebContentsObserver() = default;

void KioskWebContentsObserver::OnDidAddMessageToConsole(
    content::RenderFrameHost* source_frame,
    blink::mojom::ConsoleMessageLevel log_level,
    const std::u16string& message,
    int32_t line_no,
    const std::u16string& source_id,
    const std::optional<std::u16string>& untrusted_stack_trace) {
  logger_callback_.Run(
      {message, log_level, line_no, source_id, untrusted_stack_trace});
}

}  // namespace chromeos
