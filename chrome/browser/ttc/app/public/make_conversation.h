// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_APP_PUBLIC_MAKE_CONVERSATION_H_
#define CHROME_BROWSER_TTC_APP_PUBLIC_MAKE_CONVERSATION_H_

#include <memory>

namespace ttc {

class AudioController;
class Conversation;
class SessionController;
class TtcBackend;

// Creates the concrete ConversationImpl object for use from outside of app/.
// The conversation talks to the production (MES) backend and audio devices.
std::unique_ptr<Conversation> MakeConversationImpl(
    SessionController& session_controller);

// As above but uses `backend` to talk to the model and `audio_controller` for
// audio. Used by tests to inject mocked/fake versions of these. Both must be
// non-null.
std::unique_ptr<Conversation> MakeConversationImplForTesting(
    SessionController& session_controller,
    std::unique_ptr<TtcBackend> backend,
    std::unique_ptr<AudioController> audio_controller);

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_APP_PUBLIC_MAKE_CONVERSATION_H_
