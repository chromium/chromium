// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_TTC_KEYED_SERVICE_H_
#define CHROME_BROWSER_TTC_CORE_TTC_KEYED_SERVICE_H_

#include <memory>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "chrome/browser/ttc/core/ttc_state.h"
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
      base::RepeatingCallback<std::unique_ptr<Conversation>(
          SessionController&)>;

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

  bool IsEnabled() const;

  TtcState GetState() const;

  void StartSession();

  // Ends the active session. After this call, session_controller() is nullptr.
  // This is a no-op if no session is currently in progress.
  void EndSession();

  base::CallbackListSubscription RegisterStateChangedCallback(
      base::RepeatingCallback<void(TtcState)> callback);

  bool is_session_active() const { return session_controller_ != nullptr; }

  Profile* profile() { return profile_; }

  // Creates the Conversation for a session. Never returns null.
  std::unique_ptr<Conversation> MakeConversation(
      base::PassKey<SessionControllerImpl>,
      SessionController& session_controller);

  SessionController* session_controller() { return session_controller_.get(); }

  base::WeakPtr<TtcKeyedService> GetWeakPtr();

 private:
  raw_ptr<Profile> profile_;
  ConversationFactory conversation_factory_;
  std::unique_ptr<SessionController> session_controller_;

  base::RepeatingCallbackList<void(TtcState)> state_changed_callbacks_;

  base::WeakPtrFactory<TtcKeyedService> weak_ptr_factory_{this};
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_TTC_KEYED_SERVICE_H_
