// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_ZERO_STATE_SUGGESTIONS_MANAGER_H_
#define CHROME_BROWSER_GLIC_GLIC_ZERO_STATE_SUGGESTIONS_MANAGER_H_

#include <optional>
#include <string>
#include <vector>

#include "chrome/browser/glic/host/context/glic_tab_data.h"

namespace contextual_cueing {
class ContextualCueingService;
}  // namespace contextual_cueing

namespace glic {
class GlicSharingManagerImpl;
class GlicWindowController;
class Host;

// A class for managing sending zero state suggestions through the mojo api.
class GlicZeroStateSuggestionsManager {
 public:
  explicit GlicZeroStateSuggestionsManager(
      GlicSharingManagerImpl* sharing_manager,
      GlicWindowController* window_controller,
      contextual_cueing::ContextualCueingService* contextual_cueing_service,
      Host* host);
  virtual ~GlicZeroStateSuggestionsManager();

  // Callback to send zero state suggestions to the webui on tab changes.
  void NotifyZeroStateSuggestionsOnFocusedTabDataChanged(
      bool is_first_run,
      const std::vector<std::string>& supported_tools,
      const mojom::TabData* focused_tab_data);

  // Callback to send zero state suggestions to the webui on pinned tab changes.
  void NotifyZeroStateSuggestionsOnPinnedTabChanged(
      bool is_first_run,
      const std::vector<std::string>& supported_tools,
      const std::vector<content::WebContents*>& pinned_tab_data);

  // Callback to send zero state suggestions to the webui when pinned tab data
  // changes.
  void NotifyZeroStateSuggestionsOnPinnedTabDataChanged(
      bool is_first_run,
      const std::vector<std::string>& supported_tools,
      const mojom::TabData* data);

  // This handles calls from the webui to return a suggestion, and begin to
  // notify the webui of changes to the zero state suggestsions.
  void ObserveZeroStateSuggestions(
      bool is_notifying,
      bool is_first_run,
      const std::vector<std::string>& supported_tools,
      glic::mojom::WebClientHandler::GetZeroStateSuggestionsAndSubscribeCallback
          callback);

  void Reset();

 private:
  // A helper function to route GetZeroStateSuggestionsForFocusedTabCallback
  // callbacks.
  void OnZeroStateSuggestionsFetched(
      mojom::WebClientHandler::GetZeroStateSuggestionsAndSubscribeCallback
          callback,
      std::vector<std::string> returned_suggestions);

  // A helper function to route NotifyZeroStateSuggestions callbacks.
  void OnZeroStateSuggestionsNotify(
      bool is_first_run,
      const std::vector<std::string>& supported_tools,
      std::vector<std::string> returned_suggestions);

  base::WeakPtr<GlicZeroStateSuggestionsManager> GetWeakPtr();

  // Owned by the glic_keyed_service.
  raw_ptr<GlicSharingManagerImpl> sharing_manager_;
  raw_ptr<GlicWindowController> window_controller_;
  raw_ptr<Host> host_;

  // This passed by the glic_keyed_service.
  raw_ptr<contextual_cueing::ContextualCueingService>
      contextual_cueing_service_;

  mojom::ZeroStateSuggestionsOptions current_zero_state_suggestions_options_;
  base::CallbackListSubscription
      current_zero_state_suggestions_focus_change_subscription_;

  base::CallbackListSubscription
      current_zero_state_suggestions_pinned_tab_change_subscription_;

  base::CallbackListSubscription
      current_zero_state_suggestions_pinned_tab_data_change_subscription_;

  bool pause_pinned_subscription_updates_ = false;

  base::WeakPtrFactory<GlicZeroStateSuggestionsManager> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_ZERO_STATE_SUGGESTIONS_MANAGER_H_
