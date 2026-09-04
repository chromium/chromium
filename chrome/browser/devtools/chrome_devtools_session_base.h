// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_CHROME_DEVTOOLS_SESSION_BASE_H_
#define CHROME_BROWSER_DEVTOOLS_CHROME_DEVTOOLS_SESSION_BASE_H_

#include <memory>

#include "base/callback_list.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/devtools/protocol/protocol.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/devtools_manager_delegate.h"

namespace content {
class DevToolsAgentHostClientChannel;
}  // namespace content

class ChromeDevToolsSessionBase : public protocol::FrontendChannel {
 public:
  explicit ChromeDevToolsSessionBase(
      content::DevToolsAgentHostClientChannel* channel);

  ChromeDevToolsSessionBase(const ChromeDevToolsSessionBase&) = delete;
  ChromeDevToolsSessionBase& operator=(const ChromeDevToolsSessionBase&) =
      delete;

  ~ChromeDevToolsSessionBase() override;

  void HandleCommand(
      base::span<const uint8_t> message,
      content::DevToolsManagerDelegate::NotHandledCallback callback);

 protected:
  protocol::UberDispatcher* dispatcher() { return &dispatcher_; }

 private:
  // protocol::FrontendChannel:
  void SendProtocolResponse(
      int call_id,
      std::unique_ptr<protocol::Serializable> message) override;
  void SendProtocolNotification(
      std::unique_ptr<protocol::Serializable> message) override;
  void FlushProtocolNotifications() override;

  void OnDevToolsPolicyChanged();

  protocol::UberDispatcher dispatcher_;
  raw_ptr<content::DevToolsAgentHostClientChannel> client_channel_;
  PrefChangeRegistrar pref_change_registrar_;
  base::CallbackListSubscription policy_checker_callback_subscription_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_CHROME_DEVTOOLS_SESSION_BASE_H_
