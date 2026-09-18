// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_APP_PUBLIC_MAKE_CONVERSATION_H_
#define CHROME_BROWSER_TTC_APP_PUBLIC_MAKE_CONVERSATION_H_

#include <memory>

namespace ttc {

class Conversation;
class SessionController;

// Creates the concrete ConversationImpl object for use from outside of app/.
std::unique_ptr<Conversation> MakeConversationImpl(
    SessionController& session_controller);

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_APP_PUBLIC_MAKE_CONVERSATION_H_
