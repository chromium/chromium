// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MESSAGING_CHROME_MESSAGING_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_API_MESSAGING_CHROME_MESSAGING_DELEGATE_H_

#include "extensions/browser/api/messaging/messaging_delegate.h"
#include "extensions/common/extension_id.h"

namespace extensions {

// Helper class for Chrome-specific features of the extension messaging API.
class ChromeMessagingDelegate : public MessagingDelegate {
 public:
  ChromeMessagingDelegate();

  ChromeMessagingDelegate(const ChromeMessagingDelegate&) = delete;
  ChromeMessagingDelegate& operator=(const ChromeMessagingDelegate&) = delete;

  ~ChromeMessagingDelegate() override;

  // MessagingDelegate:
  PolicyPermission IsNativeMessagingHostAllowed(
      content::BrowserContext* browser_context,
      const std::string& native_host_name) override;
  std::optional<base::Value::Dict> MaybeGetTabInfo(
      content::WebContents* web_contents) override;
  content::WebContents* GetWebContentsByTabId(
      content::BrowserContext* browser_context,
      int tab_id) override;
  std::unique_ptr<MessagePort> CreateReceiverForTab(
      base::WeakPtr<MessagePort::ChannelDelegate> channel_delegate,
      const ExtensionId& extension_id,
      const PortId& receiver_port_id,
      content::WebContents* receiver_contents,
      int receiver_frame_id,
      const std::string& receiver_document_id) override;
  std::unique_ptr<MessagePort> CreateReceiverForNativeApp(
      content::BrowserContext* browser_context,
      base::WeakPtr<MessagePort::ChannelDelegate> channel_delegate,
      content::RenderFrameHost* source,
      const ExtensionId& extension_id,
      const PortId& receiver_port_id,
      const std::string& native_app_name,
      bool allow_user_level,
      std::string* error_out) override;
  void QueryIncognitoConnectability(
      content::BrowserContext* context,
      const Extension* extension,
      content::WebContents* web_contents,
      const GURL& url,
      base::OnceCallback<void(bool)> callback) override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MESSAGING_CHROME_MESSAGING_DELEGATE_H_
