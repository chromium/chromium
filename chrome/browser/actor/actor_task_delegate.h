// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_TASK_DELEGATE_H_
#define CHROME_BROWSER_ACTOR_ACTOR_TASK_DELEGATE_H_

#include "base/functional/callback.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/common/actor_webui.mojom.h"
#include "components/autofill/core/browser/integrators/glic/actor_form_filling_types.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/gfx/image/image.h"
#include "url/origin.h"

namespace actor {

// Delegate interface for ActorTask to communicate with other classes.
class ActorTaskDelegate {
 public:
  virtual ~ActorTaskDelegate() = default;

  // Called asynchronously when a tab is added to a task.
  virtual void OnTabAddedToTask(
      TaskId task_id,
      const tabs::TabInterface::Handle& tab_handle) = 0;

  using CredentialSelectedCallback =
      base::OnceCallback<void(webui::mojom::SelectCredentialDialogResponsePtr)>;
  virtual void RequestToShowCredentialSelectionDialog(
      TaskId task_id,
      const base::flat_map<std::string, gfx::Image>& icons,
      const std::vector<actor_login::Credential>& credentials,
      CredentialSelectedCallback callback) = 0;

  using UserConfirmationDialogCallback =
      base::OnceCallback<void(webui::mojom::UserConfirmationDialogResponsePtr)>;
  virtual void RequestToShowUserConfirmationDialog(
      TaskId task_id,
      const url::Origin& navigation_origin,
      bool for_blocklisted_origin,
      UserConfirmationDialogCallback callback) = 0;

  using NavigationConfirmationCallback =
      base::OnceCallback<void(webui::mojom::NavigationConfirmationResponsePtr)>;
  virtual void RequestToConfirmNavigation(
      TaskId task_id,
      const url::Origin& navigation_origin,
      NavigationConfirmationCallback callback) = 0;

  using AutofillSuggestionSelectedCallback = base::OnceCallback<void(
      webui::mojom::SelectAutofillSuggestionsDialogResponsePtr)>;
  virtual void RequestToShowAutofillSuggestionsDialog(
      actor::TaskId task_id,
      std::vector<autofill::ActorFormFillingRequest> requests,
      AutofillSuggestionSelectedCallback callback) = 0;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_TASK_DELEGATE_H_
