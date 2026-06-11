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

class Profile;

class BrowserWindowInterface;

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

  size_t GetUpdatesHandlerMapSizeForTesting() const {
    return context_id_to_updates_handler_map_.size();
  }

 protected:
  // Virtual for testing purposes to allow mocking the active tab.
  virtual tabs::TabInterface* GetActiveTab() const;
  // Virtual for testing purposes to allow mocking the browser window.
  virtual BrowserWindowInterface* GetBrowserWindow() const;

 private:
  friend class ExperimentalTriggeringUpdatesHandler;

  void OnUpdatesHandlerCleanup(std::string context_id);

  // Returns true if the incoming experimental triggering version is supported
  // by the client. Returns false if the incoming version is newer than the
  // client version, or if the client version is unavailable.
  bool IsVersionSupported(int incoming_version) const;

  // Returns the local experimental triggering version supported by the client.
  // Returns std::nullopt if the version is unavailable (e.g. if the user is in
  // the kUnavailable state).
  std::optional<int> GetLocalTriggeringVersion() const;

  const raw_ptr<Profile> profile_;
  const raw_ptr<SharingMessageSender> message_sender_;
  std::map<std::string, std::unique_ptr<ExperimentalTriggeringUpdatesHandler>>
      context_id_to_updates_handler_map_;
  base::WeakPtrFactory<GlicExperimentalTriggeringMessageHandler>
      weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SHARING_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_MESSAGE_HANDLER_H_
