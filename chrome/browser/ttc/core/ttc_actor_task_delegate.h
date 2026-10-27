// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_TTC_ACTOR_TASK_DELEGATE_H_
#define CHROME_BROWSER_TTC_CORE_TTC_ACTOR_TASK_DELEGATE_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/actor/actor_task_delegate.h"

namespace ttc {

// The ActorTaskDelegate of a TTC session's actor task.
//
// Navigation confirmation requests are granted (see
// RequestToConfirmNavigation()), and user confirmation dialog requests are
// declined, since TTC has no UI for them. The other dialog requests are only
// made by tools that TTC doesn't expose, so they're NOTREACHED().
class TtcActorTaskDelegate : public actor::ActorTaskDelegate {
 public:
  TtcActorTaskDelegate();
  TtcActorTaskDelegate(const TtcActorTaskDelegate&) = delete;
  TtcActorTaskDelegate& operator=(const TtcActorTaskDelegate&) = delete;
  ~TtcActorTaskDelegate() override;

  base::WeakPtr<TtcActorTaskDelegate> GetWeakPtr();

  // actor::ActorTaskDelegate:
  void OnTabAddedToTask(actor::TaskId task_id,
                        const tabs::TabInterface::Handle& tab_handle) override;
  void OnTaskTabsVisibilityChanged(actor::TaskId task_id,
                                   bool has_visible_tab) override;
  void RequestToShowCredentialSelectionDialog(
      actor::TaskId task_id,
      const base::flat_map<std::string, gfx::Image>& icons,
      const std::vector<actor_login::Credential>& credentials,
      CredentialSelectedCallback callback) override;
  // Declines every request, as for a task without a delegate. Unlike the other
  // dialog requests, this is reachable through any navigation or page action,
  // e.g. one targeting a sensitive origin.
  void RequestToShowUserConfirmationDialog(
      actor::TaskId task_id,
      const url::Origin& navigation_origin,
      bool for_blocklisted_origin,
      UserConfirmationDialogCallback callback) override;
  // Grants every request, so every navigation to an origin the task hasn't
  // visited proceeds without any user or server confirmation. A navigation
  // reaches this only after the actor framework's other origin gating checks
  // (Safe Browsing, the actor safety lists, enterprise policy, lookalike and
  // sensitive-origin checks) found no reason to block or prompt for it, and
  // only when `confirm_navigation_to_new_origins` is enabled. Glic answers this
  // by asking its server whether the navigation fits the user's task; TTC has
  // no such check, so without this the actor framework would deny every such
  // navigation, e.g. following a search result.
  // TODO(b/578062960): Decide how TTC should confirm these navigations.
  void RequestToConfirmNavigation(
      actor::TaskId task_id,
      const url::Origin& navigation_origin,
      NavigationConfirmationCallback callback) override;
  void RequestToShowAutofillSuggestionsDialog(
      actor::TaskId task_id,
      std::vector<autofill::ActorFormFillingRequest> requests,
      base::WeakPtr<actor::AutofillSelectionDialogEventHandler> event_handler,
      AutofillSuggestionSelectedCallback callback) override;
  void RequestToShowGmailOtpOptInDialog(
      actor::TaskId task_id,
      GmailOtpOptInCallback callback) override;
  void RequestToShowGmailOtpConfirmationDialog(
      actor::TaskId task_id,
      const std::string& verification_code,
      GmailOtpConfirmationCallback callback) override;

 private:
  base::WeakPtrFactory<TtcActorTaskDelegate> weak_ptr_factory_{this};
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_TTC_ACTOR_TASK_DELEGATE_H_
