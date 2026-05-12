// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_SHARING_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_MESSAGE_HANDLER_H_

#include <map>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "components/sharing_message/sharing_message_handler.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"

namespace glic {
class GlicExperimentalOptInController;
}  // namespace glic

class Profile;

class SharingMessageSender;

namespace tabs {
class TabInterface;
}
class ExperimentalTriggeringUpdatesHandler;

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
  friend class ExperimentalTriggeringUpdatesHandler;

  void ProcessDeviceOptInRequest(tabs::TabInterface* active_tab);

  void OnUpdatesHandlerCleanup(std::string context_id);

  const raw_ptr<Profile> profile_;
  const raw_ptr<SharingMessageSender> message_sender_;
#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<glic::GlicExperimentalOptInController> opt_in_controller_;
#endif
  std::map<std::string, std::unique_ptr<ExperimentalTriggeringUpdatesHandler>>
      context_id_to_updates_handler_map_;
  base::WeakPtrFactory<GlicExperimentalTriggeringMessageHandler>
      weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SHARING_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_MESSAGE_HANDLER_H_
