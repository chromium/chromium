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
#include "base/supports_user_data.h"
#include "base/types/pass_key.h"
#include "chrome/browser/ttc/app/public/audio_controller.h"
#include "chrome/browser/ttc/core/states.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace ttc {

class Conversation;
class SessionController;
class SessionControllerImpl;
class TtcActorUiStateManager;

// SupportsUserData lets platform-specific bridges (e.g. the Android JNI
// bridge) attach themselves to the service so they share its lifetime.
class TtcKeyedService : public KeyedService, public base::SupportsUserData {
 public:
  // Creates the Conversation used by a session.
  using ConversationFactory = base::RepeatingCallback<std::unique_ptr<
      Conversation>(SessionController&, std::unique_ptr<AudioController>)>;

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

  ServiceState GetState() const;

  // Starts a new session. Caller must ensure IsEnabled() is true and no session
  // is currently active. If `audio_controller` is provided, it is used instead
  // of the default production AudioControllerImpl.
  void StartSession(
      std::unique_ptr<AudioController> audio_controller = nullptr);

  // Ends the active session. After this call, session_controller() is nullptr.
  // This is a no-op if no session is currently in progress.
  void EndSession();

  base::CallbackListSubscription RegisterStateChangedCallback(
      base::RepeatingCallback<void(ServiceState)> callback);

  bool is_session_active() const { return session_controller_ != nullptr; }

  Profile* profile() { return profile_; }

  // Creates the Conversation for a session. Never returns null.
  std::unique_ptr<Conversation> MakeConversation(
      base::PassKey<SessionControllerImpl>,
      SessionController& session_controller,
      std::unique_ptr<AudioController> audio_controller = nullptr);

  // The active session's controller, or null if there's none.
  SessionController* session_controller();

  // Mutes or unmutes the active session's microphone (see
  // SessionControllerImpl::SetMicMuted()). Returns whether the state changed:
  // false if there's no live session, or the microphone already was in that
  // state.
  bool SetMicMuted(bool muted);
  // Whether the active session's microphone is muted. False if there's no live
  // session.
  bool IsMicMuted() const;

  // The ActorUiStateManagerInterface implementation used by actor tasks created
  // for TTC sessions.
  TtcActorUiStateManager& actor_ui_state_manager();

  base::WeakPtr<TtcKeyedService> GetWeakPtr();

 private:
  raw_ptr<Profile> profile_;
  ConversationFactory conversation_factory_;
  // Must outlive `session_controller_`, which indirectly holds a reference to
  // it, as do the actor tasks created during a session.
  std::unique_ptr<TtcActorUiStateManager> actor_ui_state_manager_;
  std::unique_ptr<SessionControllerImpl> session_controller_;

  base::RepeatingCallbackList<void(ServiceState)> state_changed_callbacks_;

  base::WeakPtrFactory<TtcKeyedService> weak_ptr_factory_{this};
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_TTC_KEYED_SERVICE_H_
