// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CRITICAL_ACTIONS_GLIC_LINKOUT_HANDLER_H_
#define CHROME_BROWSER_CRITICAL_ACTIONS_GLIC_LINKOUT_HANDLER_H_

#include "base/no_destructor.h"
#include "chrome/browser/critical_actions/chat_linkout_handler.h"

class Profile;

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace critical_actions {

// Handles opening Gemini conversations inside the Glic sidebar panel.
class GlicLinkoutHandler : public ChatLinkoutHandler {
 public:
  static GlicLinkoutHandler* GetInstance();

  GlicLinkoutHandler(const GlicLinkoutHandler&) = delete;
  GlicLinkoutHandler& operator=(const GlicLinkoutHandler&) = delete;

  // ChatLinkoutHandler:
  // Only `kHistoryPageCriticalActionChatLinkout` passes
  // `entry.conversation_anchor_id` to Glic, as the `ConversationId` turn ID.
  // Every other source, including the chrome://history entry-menu
  // `kHistoryPageChatLinkout`, passes no turn ID.
  void OpenConversation(
      content::WebContents* web_contents,
      const CriticalActionEntry& entry,
      glic::mojom::InvocationSource source,
      base::OnceCallback<void(OpenConversationResult)> callback) override;

  // Opens the conversation associated with `entry` on `tab`. If `tab` belongs
  // to a profile other than `profile` (e.g. it is an Incognito tab), no
  // conversation is opened, the result is `kErrorInternal` and a crash dump is
  // reported without crashing. On Android, if `tab` is null (e.g. when invoked
  // from a separate Activity such as HistoryActivity), opens it on the active
  // tab of the most recently active tabbed browser window of `profile`, or on a
  // new tab in that window if it has none. If `profile` has no tabbed browser
  // window, no conversation is opened and the result is `kErrorInternal`.
  // `kSuccess` means that Glic was invoked, not that it is shown.
  // Like the overload above, only `kHistoryPageCriticalActionChatLinkout`
  // passes `entry.conversation_anchor_id` to Glic as the turn ID.
  void OpenConversation(
      Profile* profile,
      tabs::TabInterface* tab,
      const CriticalActionEntry& entry,
      glic::mojom::InvocationSource source,
      base::OnceCallback<void(OpenConversationResult)> callback);

 private:
  friend class base::NoDestructor<GlicLinkoutHandler>;

  GlicLinkoutHandler();
  ~GlicLinkoutHandler() override;
};

}  // namespace critical_actions

#endif  // CHROME_BROWSER_CRITICAL_ACTIONS_GLIC_LINKOUT_HANDLER_H_
