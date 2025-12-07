// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_CLIENT_SERVICE_H_
#define CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_CLIENT_SERVICE_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/send_tab_to_self/receiving_ui_handler.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_model_observer.h"

namespace send_tab_to_self {

class ReceivingUiHandler;
class SendTabToSelfEntry;
class SendTabToSelfModel;

// Service that listens for SendTabToSelf model changes and calls UI
// handlers to update the UI accordingly.
class SendTabToSelfClientService : public KeyedService,
                                   public SendTabToSelfModelObserver {
 public:
  // `model` must outlive this object. `receiving_ui_handler` must be usable
  // until this keyed service is Shutdown() (in particular it cannot depend on
  // any services that are instantiated after this one).
  SendTabToSelfClientService(
      std::unique_ptr<ReceivingUiHandler> receiving_ui_handler,
      SendTabToSelfModel* model);

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

  // Returns the registered ReceivingUiHandler.
  ReceivingUiHandler* GetReceivingUiHandler() const;

 private:
  // The model outlives this object, so this is fine.
  base::ScopedObservation<SendTabToSelfModel, SendTabToSelfModelObserver>
      model_observation_{this};
  // Reset on Shutdown().
  std::unique_ptr<ReceivingUiHandler> receiving_ui_handler_;
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_CLIENT_SERVICE_H_
