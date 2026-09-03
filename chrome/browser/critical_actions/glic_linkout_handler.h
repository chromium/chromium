// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CRITICAL_ACTIONS_GLIC_LINKOUT_HANDLER_H_
#define CHROME_BROWSER_CRITICAL_ACTIONS_GLIC_LINKOUT_HANDLER_H_

#include "base/no_destructor.h"
#include "chrome/browser/critical_actions/chat_linkout_handler.h"

namespace critical_actions {

// Handles opening Gemini conversations inside the Glic sidebar panel.
class GlicLinkoutHandler : public ChatLinkoutHandler {
 public:
  static GlicLinkoutHandler* GetInstance();

  GlicLinkoutHandler(const GlicLinkoutHandler&) = delete;
  GlicLinkoutHandler& operator=(const GlicLinkoutHandler&) = delete;

  // ChatLinkoutHandler:
  void OpenConversation(
      content::WebContents* web_contents,
      const CriticalActionEntry& entry,
      glic::mojom::InvocationSource source,
      base::OnceCallback<void(OpenConversationResult)> callback) override;

 private:
  friend class base::NoDestructor<GlicLinkoutHandler>;

  GlicLinkoutHandler();
  ~GlicLinkoutHandler() override;
};

}  // namespace critical_actions

#endif  // CHROME_BROWSER_CRITICAL_ACTIONS_GLIC_LINKOUT_HANDLER_H_
