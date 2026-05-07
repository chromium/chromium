// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_SHARING_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_MESSAGE_HANDLER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "components/sharing_message/sharing_message_handler.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"

namespace glic {
class GlicExperimentalOptInController;
class GlicInstance;
class GlicKeyedService;
}  // namespace glic

class Profile;

class SharingMessageSender;

namespace tabs {
class TabInterface;
}

class GlicExperimentalTriggeringMessageHandler : public SharingMessageHandler {
 public:
  GlicExperimentalTriggeringMessageHandler(
      Profile* profile,
      SharingMessageSender* message_sender);
  GlicExperimentalTriggeringMessageHandler(
      const GlicExperimentalTriggeringMessageHandler&) = delete;
  GlicExperimentalTriggeringMessageHandler& operator=(
      const GlicExperimentalTriggeringMessageHandler&) = delete;
  ~GlicExperimentalTriggeringMessageHandler() override;

  void OnMessage(components_sharing_message::SharingMessage message,
                 DoneCallback done_callback) override;

 protected:
  // Virtual for testing purposes to allow mocking the active tab.
  virtual tabs::TabInterface* GetActiveTab() const;

 private:
  void OnClientConnectedForUpdates(
      components_sharing_message::ServerChannelConfiguration server_channel,
      std::optional<int64_t> last_seen_sequence_number,
      base::WeakPtr<glic::GlicInstance> instance);

  void ProcessDeviceOptInRequest(
      components_sharing_message::SharingMessage message,
      tabs::TabInterface* active_tab,
      DoneCallback done_callback);

  void ProcessStopActionRequest(
      components_sharing_message::SharingMessage message,
      tabs::TabInterface* active_tab,
      glic::GlicKeyedService* glic_service,
      DoneCallback done_callback);

  // Checks if experimental triggering is allowed for the profile. If NOT
  // allowed, handles the rejection by logging metrics, sending a FAILED
  // response back to the server (if FCM is configured), invoking the
  // `done_callback`, and returning true to indicate the message has been fully
  // handled. If allowed, returns false to indicate that normal handling should
  // proceed.
  bool HandleUnavailableExperimentalTriggering(
      glic::GlicKeyedService* glic_service,
      const components_sharing_message::SharingMessage& message,
      SharingMessageHandler::DoneCallback& done_callback);

  const raw_ptr<Profile> profile_;

  const raw_ptr<SharingMessageSender> message_sender_;
  mojo::UniqueReceiverSet<glic::mojom::ExperimentalTriggeringUpdatesHandler>
      listeners_;
#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<glic::GlicExperimentalOptInController> opt_in_controller_;
#endif
  base::WeakPtrFactory<GlicExperimentalTriggeringMessageHandler>
      weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SHARING_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_MESSAGE_HANDLER_H_
