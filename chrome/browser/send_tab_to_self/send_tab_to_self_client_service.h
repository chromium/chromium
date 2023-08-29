// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_CLIENT_SERVICE_H_
#define CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_CLIENT_SERVICE_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/send_tab_to_self/receiving_ui_handler.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_model_observer.h"

class Profile;

namespace send_tab_to_self {
class ReceivingUiHandlerRegistry;
class SendTabToSelfEntry;
class SendTabToSelfModel;

// Service that listens for SendTabToSelf model changes and calls UI
// handlers to update the UI accordingly.
class SendTabToSelfClientService : public KeyedService,
                                   public SendTabToSelfModelObserver {
 public:
  SendTabToSelfClientService(Profile* profile, SendTabToSelfModel* model);

  SendTabToSelfClientService(const SendTabToSelfClientService&) = delete;
  SendTabToSelfClientService& operator=(const SendTabToSelfClientService&) =
      delete;
  ~SendTabToSelfClientService() override;

  void Shutdown() override;

  // Keeps track of when the model is loaded so that updates to the
  // model can be pushed afterwards.
  void SendTabToSelfModelLoaded() override;
  // Updates the UI to reflect the new entries. Calls the handlers
  // registered through ReceivingUIRegistry.
  void EntriesAddedRemotely(
      const std::vector<const SendTabToSelfEntry*>& new_entries) override;
  // Updates the UI to reflect the removal of entries. Calls the handlers
  // registered through ReceivingUIRegistry.
  void EntriesRemovedRemotely(const std::vector<std::string>& guids) override;

 protected:

  // Sets up the ReceivingUiHandlerRegistry.
  virtual void SetupHandlerRegistry(Profile* profile);

  // Returns a vector containing the registered ReceivingUiHandlers.
  virtual const std::vector<std::unique_ptr<ReceivingUiHandler>>& GetHandlers()
      const;

 private:
  // Owned by the SendTabToSelfSyncService which should outlive this class
  raw_ptr<SendTabToSelfModel> model_;
  // Singleton instance not owned by this class
  raw_ptr<ReceivingUiHandlerRegistry> registry_;
  // Profile for which this service is associated.
  raw_ptr<Profile> profile_;
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_CLIENT_SERVICE_H_
