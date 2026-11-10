// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_COORDINATOR_H_
#define CHROME_BROWSER_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_COORDINATOR_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_metrics.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_types.h"

class Profile;
class BrowserWindowInterface;

namespace components_sharing_message {
class GlicExperimentalTriggering;
}  // namespace components_sharing_message

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace glic {

class ExperimentalTriggeringUpdatesHandler;

// Coordinates experimental triggering requests and life-cycle updates for a
// profile. Responsible for processing triggering requests, invoking Glic
// auto-submit operations, and handling task state updates.
class GlicExperimentalTriggeringCoordinator
    : public actor::ActorKeyedService::BackgroundActuationObserver {
 public:
  explicit GlicExperimentalTriggeringCoordinator(Profile* profile);
  GlicExperimentalTriggeringCoordinator(
      const GlicExperimentalTriggeringCoordinator&) = delete;
  GlicExperimentalTriggeringCoordinator& operator=(
      const GlicExperimentalTriggeringCoordinator&) = delete;
  ~GlicExperimentalTriggeringCoordinator() override;

  // Validates, logs, and processes an incoming raw protobuf triggering
  // message, invoking `response_callback` with the domain response (which may
  // be deferred on Android while preparing a background tab).
  void OnProtoMessage(
      const std::string& context_id,
      const components_sharing_message::GlicExperimentalTriggering& proto,
      ScopedIncomingMessageResultLogger result_logger,
      GlicExperimentalTriggeringUpdateCallback update_callback,
      GlicExperimentalTriggeringResponseCallback response_callback =
          base::DoNothing());

  // Handles an incoming domain request for experimental triggering.
  // `result_logger` carries the responsibility for recording the
  // Glic.ExperimentalTriggering.IncomingMessageResult histogram; the
  // coordinator sets the result for every exit path it owns.
  virtual std::optional<ExperimentalTriggeringResponse> OnRequest(
      const std::string& context_id,
      const ExperimentalTriggeringRequest& request,
      ScopedIncomingMessageResultLogger result_logger,
      GlicExperimentalTriggeringUpdateCallback update_callback,
      tabs::TabInterface* prepared_tab);

  // actor::ActorKeyedService::BackgroundActuationObserver implementation:
  void OnBackgroundTabPrepared(tabs::TabInterface* tab,
                               const std::string& context_id) override;
  void OnBackgroundSetupFailed(const std::string& context_id) override;

  size_t GetUpdatesHandlerMapSizeForTesting() const {
    return context_id_to_updates_handler_map_.size();
  }

  base::WeakPtr<GlicExperimentalTriggeringCoordinator> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 protected:
  // Virtual for testing/delegation purposes to allow querying or overriding
  // active tab/window or background state.
  virtual BrowserWindowInterface* GetBrowserWindow() const;
  virtual tabs::TabInterface* GetActiveTab() const;
  virtual bool IsChromeInBackground() const;

 private:
  friend class ExperimentalTriggeringUpdatesHandler;

  struct PendingMessageData;

  std::optional<ExperimentalTriggeringResponse> ProcessValidatedProtoMessage(
      const std::string& context_id,
      const components_sharing_message::GlicExperimentalTriggering& proto,
      ScopedIncomingMessageResultLogger result_logger,
      GlicExperimentalTriggeringUpdateCallback update_callback,
      tabs::TabInterface* prepared_tab);
  std::vector<PendingMessageData> TakePendingMessages(
      const std::string& context_id);
  void FailPendingMessages(
      const std::string& context_id,
      std::vector<PendingMessageData> messages,
      GlicExperimentalTriggeringIncomingMessageResult result,
      std::string_view latency_histogram_name,
      const std::string& error_message);
  void CancelAllPendingMessages();
  void OnUpdatesHandlerCleanup(const std::string& context_id);
  // Rejects a request that OnProtoMessage() refuses before it reaches a
  // handler, and sends the FAILED reply through `response_callback`. If a
  // handler is live for `context_id`, the reply takes that handler's next
  // sender sequence number, so that the server does not drop it as stale, and
  // the handler's session ends too.
  void RejectBeforeHandler(
      const std::string& context_id,
      const components_sharing_message::GlicExperimentalTriggering& proto,
      ScopedIncomingMessageResultLogger& result_logger,
      GlicExperimentalTriggeringIncomingMessageResult result,
      const std::string& error_message,
      GlicExperimentalTriggeringResponseCallback response_callback,
      std::map<std::string, std::string> task_update_metadata = {});

  // Returns true if an active updates handler exists for `context_id`.
  bool HasUpdatesHandler(std::string_view context_id) const {
    return context_id_to_updates_handler_map_.find(context_id) !=
           context_id_to_updates_handler_map_.end();
  }

  const raw_ptr<Profile> profile_;
  std::map<std::string,
           std::unique_ptr<ExperimentalTriggeringUpdatesHandler>,
           std::less<>>
      context_id_to_updates_handler_map_;
  std::map<std::string, std::vector<PendingMessageData>, std::less<>>
      pending_messages_;
  base::ScopedObservation<actor::ActorKeyedService,
                          actor::ActorKeyedService::BackgroundActuationObserver>
      actor_service_observation_{this};
  base::WeakPtrFactory<GlicExperimentalTriggeringCoordinator> weak_ptr_factory_{
      this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_COORDINATOR_H_
