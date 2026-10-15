// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_APP_PUBLIC_MAKE_CONVERSATION_H_
#define CHROME_BROWSER_TTC_APP_PUBLIC_MAKE_CONVERSATION_H_

#include <memory>

#include "chrome/browser/ttc/app/public/audio_controller.h"

namespace ttc {

class Conversation;
class SessionController;

// Creates the concrete ConversationImpl object for use from outside of app/.
// The conversation talks to the production (MES) backend. Uses
// `audio_controller` if non-null, otherwise creates a default
// AudioControllerImpl talking to real audio devices.
std::unique_ptr<Conversation> MakeConversationImpl(
    SessionController& session_controller,
    std::unique_ptr<AudioController> audio_controller = nullptr);

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_APP_PUBLIC_MAKE_CONVERSATION_H_
