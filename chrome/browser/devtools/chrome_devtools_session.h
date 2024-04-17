// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_CHROME_DEVTOOLS_SESSION_H_
#define CHROME_BROWSER_DEVTOOLS_CHROME_DEVTOOLS_SESSION_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/devtools/protocol/extensions_handler.h"
#include "chrome/browser/devtools/protocol/protocol.h"
#include "chrome/browser/devtools/protocol/storage_handler.h"
#include "content/public/browser/devtools_manager_delegate.h"

namespace content {
class DevToolsAgentHostClientChannel;
}  // namespace content

class AutofillHandler;
class EmulationHandler;
class BrowserHandler;
class CastHandler;
class PageHandler;
class PWAHandler;
class SecurityHandler;
class StorageHandler;
class SystemInfoHandler;
class TargetHandler;
class WindowManagerHandler;

class ChromeDevToolsSession : public protocol::FrontendChannel {
 public:
  explicit ChromeDevToolsSession(
      content::DevToolsAgentHostClientChannel* channel);

  ChromeDevToolsSession(const ChromeDevToolsSession&) = delete;
  ChromeDevToolsSession& operator=(const ChromeDevToolsSession&) = delete;

  ~ChromeDevToolsSession() override;

  void HandleCommand(
      base::span<const uint8_t> message,
      content::DevToolsManagerDelegate::NotHandledCallback callback);

  TargetHandler* target_handler() { return target_handler_.get(); }

 private:
  // protocol::FrontendChannel:
  void SendProtocolResponse(
      int call_id,
      std::unique_ptr<protocol::Serializable> message) override;
  void SendProtocolNotification(
      std::unique_ptr<protocol::Serializable> message) override;
  void FlushProtocolNotifications() override;
  void FallThrough(int call_id,
                   crdtp::span<uint8_t> method,
                   crdtp::span<uint8_t> message) override;

  base::flat_map<int, content::DevToolsManagerDelegate::NotHandledCallback>
      pending_commands_;

  protocol::UberDispatcher dispatcher_;
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
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<WindowManagerHandler> window_manager_handler_;
#endif
  raw_ptr<content::DevToolsAgentHostClientChannel> client_channel_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_CHROME_DEVTOOLS_SESSION_H_
