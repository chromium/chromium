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

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_metrics.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_types.h"

class Profile;
class BrowserWindowInterface;

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace glic {

class ExperimentalTriggeringUpdatesHandler;

// Coordinates experimental triggering requests and life-cycle updates for a
// profile. Responsible for processing triggering requests, invoking Glic
// auto-submit operations, and handling task state updates.
class GlicExperimentalTriggeringCoordinator {
 public:
  explicit GlicExperimentalTriggeringCoordinator(Profile* profile);
  GlicExperimentalTriggeringCoordinator(
      const GlicExperimentalTriggeringCoordinator&) = delete;
  GlicExperimentalTriggeringCoordinator& operator=(
      const GlicExperimentalTriggeringCoordinator&) = delete;
  virtual ~GlicExperimentalTriggeringCoordinator();

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

  size_t GetUpdatesHandlerMapSizeForTesting() const {
    return context_id_to_updates_handler_map_.size();
  }

 protected:
  // Virtual for testing/delegation purposes to allow querying or overriding
  // active tab/window.
  virtual BrowserWindowInterface* GetBrowserWindow() const;
  virtual tabs::TabInterface* GetActiveTab() const;

 private:
  friend class ExperimentalTriggeringUpdatesHandler;

  void OnUpdatesHandlerCleanup(std::string_view context_id);

  const raw_ptr<Profile> profile_;
  std::map<std::string,
           std::unique_ptr<ExperimentalTriggeringUpdatesHandler>,
           std::less<>>
      context_id_to_updates_handler_map_;
  base::WeakPtrFactory<GlicExperimentalTriggeringCoordinator> weak_ptr_factory_{
      this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_COORDINATOR_H_
