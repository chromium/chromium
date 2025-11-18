// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_WEB_CLIENT_ACCESS_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_WEB_CLIENT_ACCESS_H_

// Interface to the glic web client, provided by the glic WebUI.
#include "base/functional/callback_forward.h"
#include "chrome/browser/actor/actor_task_delegate.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "components/autofill/core/browser/integrators/glic/actor_form_filling_types.h"
#include "url/origin.h"

namespace glic {

// Access to the glic web client, from outside of the WebUI handler.
class GlicWebClientAccess {
 public:
  using PanelWillOpenCallback = mojom::WebClient::NotifyPanelWillOpenCallback;

  // Informs the web client that the panel will open. The panel should not be
  // shown until `done` is called.
  virtual void PanelWillOpen(mojom::PanelOpeningDataPtr panel_opening_data,
                             PanelWillOpenCallback done) = 0;

  // Informs the web client the panel was closed (no longer visible). The web
  // client should not be destroyed until after `done` is called.
  virtual void PanelWasClosed(base::OnceClosure done) = 0;

  // Informs the client that the state of the panel has changed.
  virtual void PanelStateChanged(
      const glic::mojom::PanelState& panel_state) = 0;

  virtual void NotifyInstanceActivationChanged(bool is_active) = 0;

  // Informs the web client when the user starts and finishes dragging to resize
  // the panel.
  virtual void ManualResizeChanged(bool resizing) = 0;

  // Called when the browser wants the web client to change its view to match
  // the requested change (e.g., because the user clicked a UI element to toggle
  // to a different view).
  virtual void RequestViewChange(mojom::ViewChangeRequestPtr request) = 0;

  // Informs the web client that additional context is available.
  virtual void NotifyAdditionalContext(mojom::AdditionalContextPtr context) = 0;

  virtual void RequestToShowCredentialSelectionDialog(
      actor::TaskId task_id,
      const base::flat_map<std::string, gfx::Image>& icons,
      const std::vector<actor_login::Credential>& credentials,
      actor::ActorTaskDelegate::CredentialSelectedCallback callback) = 0;
  virtual void RequestToShowUserConfirmationDialog(
      actor::TaskId task_id,
      const url::Origin& navigation_origin,
      bool for_blocklisted_origin,
      actor::ActorTaskDelegate::UserConfirmationDialogCallback callback) = 0;
  virtual void RequestToConfirmNavigation(
      actor::TaskId task_id,
      const url::Origin& navigation_origin,
      actor::ActorTaskDelegate::NavigationConfirmationCallback callback) = 0;
  virtual void RequestToShowAutofillSuggestionsDialog(
      actor::TaskId task_id,
      std::vector<autofill::ActorFormFillingRequest> requests,
      actor::ActorTaskDelegate::AutofillSuggestionSelectedCallback
          callback) = 0;

  virtual void FloatingPanelCanAttachChanged(bool can_attach) = 0;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_WEB_CLIENT_ACCESS_H_
