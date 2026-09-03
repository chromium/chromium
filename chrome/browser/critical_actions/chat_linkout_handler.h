// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CRITICAL_ACTIONS_CHAT_LINKOUT_HANDLER_H_
#define CHROME_BROWSER_CRITICAL_ACTIONS_CHAT_LINKOUT_HANDLER_H_

#include "base/functional/callback_forward.h"
#include "chrome/common/glic_enums.mojom-forward.h"
#include "components/critical_actions/core/browser/critical_action_types.h"

namespace content {
class WebContents;
}  // namespace content

namespace critical_actions {

// Interface for handling linkouts to chat/conversation sessions from critical
// actions.
class ChatLinkoutHandler {
 public:
  virtual ~ChatLinkoutHandler() = default;

  // Handles navigation/presentation for a critical action's source context,
  // relative to the initiating `web_contents`.
  virtual void OpenConversation(
      content::WebContents* web_contents,
      const CriticalActionEntry& entry,
      glic::mojom::InvocationSource source,
      base::OnceCallback<void(OpenConversationResult)> callback) = 0;
};

}  // namespace critical_actions

#endif  // CHROME_BROWSER_CRITICAL_ACTIONS_CHAT_LINKOUT_HANDLER_H_
