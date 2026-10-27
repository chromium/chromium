// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TRANSPORT_ACTOR_KEYED_SERVICE_ADAPTER_H_
#define CHROME_BROWSER_ACTOR_TRANSPORT_ACTOR_KEYED_SERVICE_ADAPTER_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/actor/actor_keyed_service_proto_wrapper.h"
#include "chrome/browser/actor/actor_task_delegate.h"
#include "components/actor/core/task_id.h"
#include "components/actor/transport/actuation_delegate.h"
#include "components/autofill/core/browser/integrators/actor/actor_form_filling_types.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/gfx/image/image.h"
#include "url/origin.h"

namespace chrome::aix::autobrowse::proto {
class ObservationConfig;
}  // namespace chrome::aix::autobrowse::proto

namespace actor {

class ActorKeyedService;
class AutofillSelectionDialogEventHandler;
class EnterprisePolicyChecker;

// Adapts ActorKeyedService to the ActuationDelegate interface used by
// ActuationTransportHandler.
class ActorKeyedServiceAdapter : public ActuationDelegate,
                                 public ActorTaskDelegate {
 public:
  ActorKeyedServiceAdapter(
      ActorKeyedService* actor_service,
      const EnterprisePolicyChecker* enterprise_policy_checker);
  ActorKeyedServiceAdapter(const ActorKeyedServiceAdapter&) = delete;
  ActorKeyedServiceAdapter& operator=(const ActorKeyedServiceAdapter&) = delete;
  ~ActorKeyedServiceAdapter() override;

  // ActuationDelegate:
  void StartTask(const proto::StartTaskRequest& request,
                 StartTaskCallback callback) override;
  void StopTask(TaskId task_id,
                const proto::StopTaskRequest& request,
                StopTaskCallback callback) override;
  void Act(TaskId task_id,
           const proto::PerformActionsRequest& request,
           ActCallback callback) override;

  // ActorTaskDelegate:
  void OnTabAddedToTask(TaskId task_id,
                        const tabs::TabInterface::Handle& tab_handle) override;
  void OnTaskTabsVisibilityChanged(TaskId task_id,
                                   bool has_visible_tab) override;
  void RequestToShowCredentialSelectionDialog(
      TaskId task_id,
      const base::flat_map<std::string, gfx::Image>& icons,
      const std::vector<actor_login::Credential>& credentials,
      CredentialSelectedCallback callback) override;
  void RequestToShowUserConfirmationDialog(
      TaskId task_id,
      const url::Origin& navigation_origin,
      bool for_blocklisted_origin,
      UserConfirmationDialogCallback callback) override;
  void RequestToConfirmNavigation(
      TaskId task_id,
      const url::Origin& navigation_origin,
      NavigationConfirmationCallback callback) override;
  void RequestToShowAutofillSuggestionsDialog(
      TaskId task_id,
      std::vector<autofill::ActorFormFillingRequest> requests,
      base::WeakPtr<AutofillSelectionDialogEventHandler> event_handler,
      AutofillSuggestionSelectedCallback callback) override;
  void RequestToShowGmailOtpOptInDialog(
      TaskId task_id,
      GmailOtpOptInCallback callback) override;
  void RequestToShowGmailOtpConfirmationDialog(
      TaskId task_id,
      const std::string& verification_code,
      GmailOtpConfirmationCallback callback) override;

 private:
  friend class ActorKeyedServiceAdapterTest;

  static optimization_guide::proto::Actions BuildActionsProto(
      TaskId task_id,
      const proto::PerformActionsRequest& request);
  static proto::PerformActionsResponse BuildPerformActionsResponse(
      optimization_guide::proto::ActionsResult result,
      const proto::ObservationConfig* observation_config);

  // ActorKeyedService and EnterprisePolicyChecker outlive this adapter.
  raw_ptr<ActorKeyedService> actor_service_;
  raw_ptr<const EnterprisePolicyChecker> enterprise_policy_checker_;
  ActorKeyedServiceProtoWrapper actor_keyed_service_proto_wrapper_{
      actor_service_};
  base::WeakPtrFactory<ActorKeyedServiceAdapter> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TRANSPORT_ACTOR_KEYED_SERVICE_ADAPTER_H_
