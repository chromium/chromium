// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_SHARING_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_MESSAGE_HANDLER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_metrics.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "components/sharing_message/sharing_message_handler.h"

class Profile;
class BrowserWindowInterface;
class SharingMessageSender;

namespace tabs {
class TabInterface;
}

namespace glic {
class GlicExperimentalTriggeringCoordinator;
}

class GlicExperimentalTriggeringMessageHandler
    : public SharingMessageHandler,
      public actor::ActorKeyedService::BackgroundActuationObserver {
 public:
  GlicExperimentalTriggeringMessageHandler(
      Profile* profile,
      SharingMessageSender* message_sender);
  GlicExperimentalTriggeringMessageHandler(
      Profile* profile,
      SharingMessageSender* message_sender,
      std::unique_ptr<glic::GlicExperimentalTriggeringCoordinator> coordinator);
  GlicExperimentalTriggeringMessageHandler(
      const GlicExperimentalTriggeringMessageHandler&) = delete;
  GlicExperimentalTriggeringMessageHandler& operator=(
      const GlicExperimentalTriggeringMessageHandler&) = delete;
  ~GlicExperimentalTriggeringMessageHandler() override;

  void OnMessage(components_sharing_message::SharingMessage message,
                 DoneCallback done_callback) override;

  size_t GetUpdatesHandlerMapSizeForTesting() const;

 protected:
  // Virtual for testing purposes to allow mocking the active tab.
  virtual tabs::TabInterface* GetActiveTab() const;
  // Virtual for testing purposes to allow mocking the browser window.
  virtual BrowserWindowInterface* GetBrowserWindow() const;

 private:
  struct MessageData {
    components_sharing_message::SharingMessage message;
    SharingMessageHandler::DoneCallback done_callback;
    glic::ScopedIncomingMessageResultLogger result_logger;
  };

  // TODO(crbug.com/533526458): Cleanup this wrapper delegate after refactoring
  // migration completes.
  // Delegate adapter that bridges virtual GetBrowserWindow() and GetActiveTab()
  // calls back to GlicExperimentalTriggeringMessageHandler so existing
  // unit/browser test mocks can continue overriding window/tab resolution
  // without modifying tests.
  class MessageHandlerCoordinatorDelegate;

  // Returns true if the incoming experimental triggering version is supported
  // by the client. Returns false if the incoming version is newer than the
  // client version, or if the client version is unavailable.
  bool IsVersionSupported(int incoming_version) const;

  // Returns the local experimental triggering version supported by the client.
  // Returns std::nullopt if the version is unavailable (e.g. if the user is in
  // the kUnavailable state).
  std::optional<int> GetLocalTriggeringVersion() const;

  // actor::ActorKeyedService::BackgroundActuationObserver:
  void OnBackgroundTabPrepared(
      tabs::TabInterface* tab,
      const std::string& glic_trigger_message_id) override;
  void OnBackgroundSetupFailed(
      const std::string& glic_trigger_message_id) override;

  void ProcessValidatedMessage(
      components_sharing_message::SharingMessage message,
      const std::string& context_id,
      SharingMessageHandler::DoneCallback done_callback,
      glic::ScopedIncomingMessageResultLogger result_logger,
      tabs::TabInterface* prepared_tab);

  // Cancels all queued messages in `pending_messages_` and responds to their
  // `done_callback`s with a FAILED status to prevent the sender from hanging.
  // Called during handler destruction (e.g., when the profile is being
  // destroyed or Chrome is shutting down) to ensure clean lifecycle disposal.
  void CancelAllPendingMessages();

  const raw_ptr<Profile> profile_;
  const raw_ptr<SharingMessageSender> message_sender_;
  std::unique_ptr<glic::GlicExperimentalTriggeringCoordinator> coordinator_;
  std::map</*message_id*/ std::string, std::vector<MessageData>>
      pending_messages_;
  base::ScopedObservation<actor::ActorKeyedService,
                          actor::ActorKeyedService::BackgroundActuationObserver>
      actor_service_observation_{this};
  base::WeakPtrFactory<GlicExperimentalTriggeringMessageHandler>
      weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SHARING_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_MESSAGE_HANDLER_H_
