// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_WEB_CONTENTS_LOGS_COLLECTOR_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_WEB_CONTENTS_LOGS_COLLECTOR_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_level_logs_saver.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-data-view.h"

namespace chromeos {

// Observes and collect logs from provided web contents sources.
class KioskWebContentsLogsCollector {
 public:
  using LoggerCallback = base::RepeatingCallback<void(
      const KioskAppLevelLogsSaver::KioskLogMessage&)>;

  explicit KioskWebContentsLogsCollector(Profile* profile,
                                         LoggerCallback logger_callback);
  KioskWebContentsLogsCollector(const KioskWebContentsLogsCollector&) = delete;
  KioskWebContentsLogsCollector& operator=(
      const KioskWebContentsLogsCollector&) = delete;
  ~KioskWebContentsLogsCollector();

  class KioskWebContentsObserver : public content::WebContentsObserver {
   public:
    KioskWebContentsObserver(content::WebContents* web_contents,
                             LoggerCallback logger_callback,
                             base::OnceCallback<void(content::WebContents*)>
                                 web_contents_destroyed_callback);

    ~KioskWebContentsObserver() override;

    // `content::WebContentsObserver` implementation:
    void OnDidAddMessageToConsole(
        content::RenderFrameHost* source_frame,
        blink::mojom::ConsoleMessageLevel log_level,
        const std::u16string& message,
        int32_t line_no,
        const std::u16string& source_id,
        const std::optional<std::u16string>& untrusted_stack_trace) override;
    void WebContentsDestroyed() override;

   private:
    raw_ptr<content::WebContents> web_contents_;
    LoggerCallback logger_callback_;
    base::OnceCallback<void(content::WebContents*)>
        web_contents_destroyed_callback_;
  };

  void AddWebContentsToObserve(content::WebContents* web_contents);

 private:
  void OnWebContentsDestroyed(content::WebContents* web_contents);

  LoggerCallback logger_callback_;

  // TODO(b:417698708) Implement and initialise AppWindowsWebContentsCollector.
  // TODO(b:425645020) Implement and initialise BrowserWebContentsCollector.
  // TODO(b:425645764) Implement and initialise ExtensionsWebContentsCollector.

  std::unordered_map<content::WebContents*,
                     std::unique_ptr<KioskWebContentsObserver>>
      observers_map_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_WEB_CONTENTS_LOGS_COLLECTOR_H_
