// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_COMPONENT_ACTIVE_DIRECTORY_POLICY_RETRIEVER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_COMPONENT_ACTIVE_DIRECTORY_POLICY_RETRIEVER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/login_manager/policy_descriptor.pb.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "components/policy/core/common/policy_namespace.h"

namespace policy {

// Retrieves Active Directory component policy (e.g. policy for extensions) from
// Session Manager via D-Bus. For each retrieval operation a new instance should
// be created.
class ComponentActiveDirectoryPolicyRetriever {
 public:
  // Response type from a policy retrieval request for a single namespace.
  using ResponseType =
      chromeos::SessionManagerClient::RetrievePolicyResponseType;

  // Result from a policy retrieval request for a single policy namespace.
  struct RetrieveResult {
    PolicyNamespace ns;
    ResponseType response = ResponseType::SUCCESS;

    // Serialized enterprise_management::PolicyFetchResponse proto.
    std::string policy_fetch_response_blob;
  };

  // Callback to be called when all policy retrieval requests have finished.
  using RetrieveCallback =
      base::OnceCallback<void(std::vector<RetrieveResult>)>;

  // |account_type| is the type of account to load from (device, user, device
  // local account). |account_id| is the account id (empty for device,
  // cryptohome id for user and account name for device local account).
  // |namespaces| is the list of namespaces to load (e.g. extension ids).
  ComponentActiveDirectoryPolicyRetriever(
      login_manager::PolicyAccountType account_type,
      std::string account_id,
      std::vector<PolicyNamespace> namespaces,
      RetrieveCallback callback);

  ~ComponentActiveDirectoryPolicyRetriever();

  // Starts retrieving policy from Session Manager as specified in the
  // constuctor. Calls the |callback| passed to the constructor when component
  // policy for all namespaces has been fetched. The caller needs to keep this
  // instance alive until the callback is called or else outstanding requests
  // are canceled and the callback is never called. This method should only be
  // called once per instance.
  void Start();

 private:
  // Retrieves policy for the next namespace (depending on how many |results_|
  // have already been received). Calls |callback_| when all namespaces have
  // been handled.
  void RetrievePolicyForNextNamespace();

  // Called from Session Manager with the result of a policy retrieval request
  // for a single namespace. Keeps calling RetrievePolicyForNextNamespace()
  // until results for all namespaces have been received.
  void OnPolicyRetrieved(ResponseType response,
                         const std::string& policy_fetch_response_blob);

  // Constructor parameters.
  const login_manager::PolicyAccountType account_type_;
  const std::string account_id_;
  const std::vector<PolicyNamespace> namespaces_;
  RetrieveCallback callback_;

  // Tracks results and failed responses from retrieval queries.
  std::vector<RetrieveResult> results_;
  int failed_response_count_ = 0;

  // Flag to make sure Start() was only called once.
  bool start_was_called_ = false;

  base::WeakPtrFactory<ComponentActiveDirectoryPolicyRetriever>
      weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(ComponentActiveDirectoryPolicyRetriever);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_COMPONENT_ACTIVE_DIRECTORY_POLICY_RETRIEVER_H_
