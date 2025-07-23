// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_web_contents_logs_collector.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_level_logs_saver.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace chromeos {

KioskWebContentsLogsCollector::KioskWebContentsObserver::
    KioskWebContentsObserver(content::WebContents* web_contents,
                             LoggerCallback logger_callback,
                             base::OnceCallback<void(content::WebContents*)>
                                 web_contents_destroyed_callback)
    : content::WebContentsObserver(web_contents),
      web_contents_(web_contents),
      logger_callback_(std::move(logger_callback)),
      web_contents_destroyed_callback_(
          std::move(web_contents_destroyed_callback)) {}

KioskWebContentsLogsCollector::KioskWebContentsObserver::
    ~KioskWebContentsObserver() = default;

void KioskWebContentsLogsCollector::KioskWebContentsObserver::
    OnDidAddMessageToConsole(
        content::RenderFrameHost* source_frame,
        blink::mojom::ConsoleMessageLevel log_level,
        const std::u16string& message,
        int32_t line_no,
        const std::u16string& source_id,
        const std::optional<std::u16string>& untrusted_stack_trace) {
  logger_callback_.Run(
      {message, log_level, line_no, source_id, untrusted_stack_trace});
}

void KioskWebContentsLogsCollector::KioskWebContentsObserver::
    WebContentsDestroyed() {
  std::move(web_contents_destroyed_callback_).Run(web_contents_);
  // No method should be called after this as the callback above will cause the
  // destruction of this object.
}

KioskWebContentsLogsCollector::KioskWebContentsLogsCollector(
    Profile* profile,
    LoggerCallback logger_callback)
    : logger_callback_(std::move(logger_callback)) {}

KioskWebContentsLogsCollector::~KioskWebContentsLogsCollector() = default;

void KioskWebContentsLogsCollector::AddWebContentsToObserve(
    content::WebContents* web_contents) {
  if (observers_map_.contains(web_contents)) {
    return;
  }

  auto observer = std::make_unique<KioskWebContentsObserver>(
      web_contents, logger_callback_,
      base::BindOnce(&KioskWebContentsLogsCollector::OnWebContentsDestroyed,
                     base::Unretained(this)));
  observers_map_.emplace(web_contents, std::move(observer));
}

void KioskWebContentsLogsCollector::OnWebContentsDestroyed(
    content::WebContents* web_contents) {
  observers_map_.erase(web_contents);
}

}  // namespace chromeos
