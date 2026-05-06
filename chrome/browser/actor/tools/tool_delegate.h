// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_TOOL_DELEGATE_H_
#define CHROME_BROWSER_ACTOR_TOOLS_TOOL_DELEGATE_H_

#include <optional>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/actor/site_policy.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/actor_webui.mojom.h"
#include "chrome/common/buildflags.h"
#include "components/actor/public/mojom/actor_types.mojom-forward.h"
#include "components/autofill/core/browser/integrators/actor/actor_form_filling_types.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "components/tabs/public/tab_interface.h"
#include "url/gurl.h"

class Profile;

namespace actor_login {
class ActorLoginService;
}  // namespace actor_login

namespace autofill {
class ActorFormFillingService;
class ActorOneTimeTokenFillingService;
}  // namespace autofill

namespace favicon {
class FaviconService;
}  // namespace favicon

namespace gfx {
class Image;
}  // namespace gfx

namespace actor {

class AggregatedJournal;
class AutofillSelectionDialogEventHandler;
class EnterprisePolicyChecker;
class ToolRequest;

// Provides tools with functionality implemented by the code invoking the tool.
class ToolDelegate {
 public:
  virtual ~ToolDelegate() = default;

  // Returns the profile in which the task is running.
  virtual Profile& GetProfile() = 0;

  // Returns the journal so that tools may log information related to their
  // execution.
  virtual AggregatedJournal& GetJournal() = 0;
  // Returns the login service associated with the task.
  virtual actor_login::ActorLoginService& GetActorLoginService() = 0;

  // Returns the form filling service associated with the task.
  virtual autofill::ActorFormFillingService& GetActorFormFillingService() = 0;

  // Returns the OTP filling service associated with the task.
  virtual autofill::ActorOneTimeTokenFillingService&
  GetActorOneTimeTokenFillingService() = 0;

  // Returns the favicon service for the profile associated with the task.
  virtual favicon::FaviconService* GetFaviconService() = 0;

  // Returns the enterprise policy checker associated with the task.
  virtual const EnterprisePolicyChecker& GetEnterprisePolicyChecker() const = 0;

  // Invokes the given callback according to whether the tool may navigate to
  // the given URL.
  virtual void IsAcceptableNavigationDestination(
      const GURL& url,
      DecisionCallbackWithReason callback) = 0;

  // Prompts the user to select a credential from the list of credentials, and
  // with optional icons for each site or app that is associated with the
  // credential.
  // The callback is called with the selected credential or with an empty
  // credential if the user closed the prompt without making a selection.
  using CredentialSelectedCallback = base::OnceCallback<void(
      webui::mojom::SelectCredentialDialogResponsePtr response)>;
  virtual void PromptToSelectCredential(
      const std::vector<actor_login::Credential>& credentials,
      const base::flat_map<std::string, gfx::Image>& icons,
      CredentialSelectedCallback callback) = 0;

  // Combines a `credential` and the duration of the permission a user chose in
  // the account picker.
  struct CredentialWithPermission {
    CredentialWithPermission();
    CredentialWithPermission(
        const actor_login::Credential& credential,
        webui::mojom::UserGrantedPermissionDuration permission_duration);
    CredentialWithPermission(const CredentialWithPermission&);
    CredentialWithPermission(CredentialWithPermission&&);
    CredentialWithPermission& operator=(const CredentialWithPermission&);
    CredentialWithPermission& operator=(CredentialWithPermission&&);
    ~CredentialWithPermission();

    actor_login::Credential credential;
    // The duration of the permission the user gave for `credential`. As opposed
    // to actor_login::Credential::has_persistent_permission, this comes from
    // the credential picker and is not yet stored in the PWM DB.
    webui::mojom::UserGrantedPermissionDuration permission_duration;
  };
  // Sets / gets the credential that the user has chosen to allow the
  // actor to use. The selected credential can be used for multi-step login
  // within the same task.
  // `permission_duration` can be one-time or permanent. Permanent permission
  // means the `credential` can be used in future calls to the tool.
  // `affiliations_fetched` is invoked when the operation to fetch affiliated
  // domains is completed. Affiliations are fetched in order to reuse the
  // permission given to the selected `credential` in other domains that are
  // strongly affiliated.
  virtual void SetUserSelectedCredential(
      const CredentialWithPermission& credential,
      base::OnceClosure affiliations_fetched) = 0;
  virtual const std::optional<CredentialWithPermission>
  GetUserSelectedCredential(const url::Origin& request_origin) const = 0;

  // Prompts the user to select one of the autofill suggestion. Invokes the
  // callback with the chosen suggestion or empty if the prompt is closed.
  using AutofillSuggestionSelectedCallback = base::OnceCallback<void(
      webui::mojom::SelectAutofillSuggestionsDialogResponsePtr)>;
  virtual void RequestToShowAutofillSuggestions(
      std::vector<autofill::ActorFormFillingRequest> requests,
      base::WeakPtr<AutofillSelectionDialogEventHandler> event_handler,
      AutofillSuggestionSelectedCallback callback) = 0;

  // During tool execution, the tool becomes blocked on the user's attention.
  // The task still has control of the tab.
  virtual void InterruptFromTool() = 0;
  virtual void UninterruptFromTool() = 0;

  // Enqueues an action to be performed as a followup to the current action.
  virtual void EnqueueFollowupAction(std::unique_ptr<ToolRequest> action) = 0;

  // Adds a tab to the controlled tabs set. If `stop_task_on_detach` is true,
  // then the `ActorTask` will be stopped when the given tab is detached.
  virtual void AddTab(
      tabs::TabHandle tab_handle,
      bool stop_task_on_detach,
      base::OnceCallback<void(mojom::ActionResultPtr)> callback) = 0;
  // Returns true if the tab is in the controlled tabs set.
  virtual bool HasTab(tabs::TabHandle tab_handle) = 0;
  // Removes a tab from the controlled tabs set.
  virtual void RemoveTab(tabs::TabHandle tab_handle) = 0;

  // If there is an ongoing tool request, treat it as having failed with the
  // given reason.
  virtual void FailCurrentTool(mojom::ActionResultCode reason) = 0;

  virtual base::WeakPtr<actor_login::ActionSequenceDelegate>
  GetActionSequenceDelegate() = 0;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_TOOL_DELEGATE_H_
