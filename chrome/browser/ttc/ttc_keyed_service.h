// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_TTC_KEYED_SERVICE_H_
#define CHROME_BROWSER_TTC_TTC_KEYED_SERVICE_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/types/pass_key.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace ttc {

class Conversation;
class SessionController;
class SessionControllerImpl;

class TtcKeyedService : public KeyedService {
 public:
  // Creates the Conversation used by a session.
  using ConversationFactory =
      base::RepeatingCallback<std::unique_ptr<Conversation>(Profile*)>;

  static TtcKeyedService* Get(content::BrowserContext* context);

  // The default ConversationFactory creates a ConversationImpl. Tests can pass
  // in a factory to provide mock/fake objects.
  explicit TtcKeyedService(
      Profile* profile,
      ConversationFactory conversation_factory = ConversationFactory());
  TtcKeyedService(const TtcKeyedService&) = delete;
  TtcKeyedService& operator=(const TtcKeyedService&) = delete;
  ~TtcKeyedService() override;

  // KeyedService:
  void Shutdown() override;

  void StartSession();

  // Ends the active session. After this call, session_controller() is nullptr.
  // This is a no-op if no session is currently in progress.
  void EndSession();

  Profile* profile() { return profile_; }

  // Creates the Conversation for a session. Never returns null.
  std::unique_ptr<Conversation> MakeConversation(
      base::PassKey<SessionControllerImpl>);

  SessionController* session_controller() { return session_controller_.get(); }

 private:
  raw_ptr<Profile> profile_;
  ConversationFactory conversation_factory_;
  std::unique_ptr<SessionController> session_controller_;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_TTC_KEYED_SERVICE_H_
