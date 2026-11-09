// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_SHARING_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_MESSAGE_HANDLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_types.h"
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

class GlicExperimentalTriggeringMessageHandler : public SharingMessageHandler {
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

  // Virtual hooks for subclass customization (e.g.
  // BrowserActuatorMessageHandler).
  virtual void CheckFeatureFlags() const;
  virtual glic::GlicExperimentalTriggeringUpdateCallback GetUpdateCallback(
      components_sharing_message::SharingMessage& message);

 private:
  // TODO(crbug.com/533526458): Cleanup this wrapper delegate after refactoring
  // migration completes.
  // Delegate adapter that bridges virtual GetBrowserWindow() and GetActiveTab()
  // calls back to GlicExperimentalTriggeringMessageHandler so existing
  // unit/browser test mocks can continue overriding window/tab resolution
  // without modifying tests.
  class MessageHandlerCoordinatorDelegate;

  const raw_ptr<Profile> profile_;
  const raw_ptr<SharingMessageSender> message_sender_;
  std::unique_ptr<glic::GlicExperimentalTriggeringCoordinator> coordinator_;
  base::WeakPtrFactory<GlicExperimentalTriggeringMessageHandler>
      weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SHARING_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_MESSAGE_HANDLER_H_
