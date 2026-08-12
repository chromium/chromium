// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_CHROME_DEVTOOLS_SESSION_H_
#define CHROME_BROWSER_DEVTOOLS_CHROME_DEVTOOLS_SESSION_H_

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/devtools/chrome_devtools_session_base.h"

namespace content {
class DevToolsAgentHostClientChannel;
}  // namespace content

class AdsHandler;
class AutofillHandler;
class EmulationHandler;
class BrowserHandler;
class CastHandler;
class ExtensionsHandler;
class PageHandler;
class PWAHandler;
class SecurityHandler;
class StorageHandler;
class SystemInfoHandler;
class TargetHandler;
class WebMCPHandler;
class WindowManagerHandler;

class ChromeDevToolsSession : public ChromeDevToolsSessionBase {
 public:
  explicit ChromeDevToolsSession(
      content::DevToolsAgentHostClientChannel* channel);

  ChromeDevToolsSession(const ChromeDevToolsSession&) = delete;
  ChromeDevToolsSession& operator=(const ChromeDevToolsSession&) = delete;

  ~ChromeDevToolsSession() override;

  TargetHandler* target_handler() { return target_handler_.get(); }

 private:
  std::unique_ptr<AdsHandler> ads_handler_;
  std::unique_ptr<AutofillHandler> autofill_handler_;
  std::unique_ptr<ExtensionsHandler> extensions_handler_;
  std::unique_ptr<BrowserHandler> browser_handler_;
  std::unique_ptr<CastHandler> cast_handler_;
  std::unique_ptr<EmulationHandler> emulation_handler_;
  std::unique_ptr<PageHandler> page_handler_;
  std::unique_ptr<PWAHandler> pwa_handler_;
  std::unique_ptr<SecurityHandler> security_handler_;
  std::unique_ptr<StorageHandler> storage_handler_;
  std::unique_ptr<SystemInfoHandler> system_info_handler_;
  std::unique_ptr<TargetHandler> target_handler_;
  std::unique_ptr<WebMCPHandler> webmcp_handler_;
#if BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<WindowManagerHandler> window_manager_handler_;
#endif
};

#endif  // CHROME_BROWSER_DEVTOOLS_CHROME_DEVTOOLS_SESSION_H_
