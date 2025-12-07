// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_WEB_CONTENTS_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_WEB_CONTENTS_OBSERVER_H_

#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_level_logs_saver.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-data-view.h"

namespace chromeos {

// Observes and collect logs from provided web contents sources.
class KioskWebContentsObserver : public content::WebContentsObserver {
 public:
  using LoggerCallback = base::RepeatingCallback<void(
      const KioskAppLevelLogsSaver::KioskLogMessage&)>;

  KioskWebContentsObserver(content::WebContents* web_contents,
                           LoggerCallback logger_callback);

  ~KioskWebContentsObserver() override;

  // `content::WebContentsObserver` implementation:
  void OnDidAddMessageToConsole(
      content::RenderFrameHost* source_frame,
      blink::mojom::ConsoleMessageLevel log_level,
      const std::u16string& message,
      int32_t line_no,
      const std::u16string& source_id,
      const std::optional<std::u16string>& untrusted_stack_trace) override;

 private:
  LoggerCallback logger_callback_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_WEB_CONTENTS_OBSERVER_H_
