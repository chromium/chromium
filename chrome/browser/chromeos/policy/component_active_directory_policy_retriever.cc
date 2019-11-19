// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/component_active_directory_policy_retriever.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/policy_namespace.h"

namespace policy {
namespace {

// Maps from Chrome's PolicyDomain to SessionManager's PolicyDomain. The two
// enums are kept separate since Chrome's PolicyDomain has conceptually a larger
// scope and is used outside of Chrome OS.
login_manager::PolicyDomain MapPolicyDomain(PolicyDomain domain) {
  switch (domain) {
    case POLICY_DOMAIN_CHROME:
      return login_manager::POLICY_DOMAIN_CHROME;
    case POLICY_DOMAIN_EXTENSIONS:
      return login_manager::POLICY_DOMAIN_EXTENSIONS;
    case POLICY_DOMAIN_SIGNIN_EXTENSIONS:
      return login_manager::POLICY_DOMAIN_SIGNIN_EXTENSIONS;
    case POLICY_DOMAIN_SIZE:
      break;
  }
  NOTREACHED();
  return login_manager::POLICY_DOMAIN_CHROME;
}

}  // namespace

ComponentActiveDirectoryPolicyRetriever::
    ComponentActiveDirectoryPolicyRetriever(
        login_manager::PolicyAccountType account_type,
        std::string account_id,
        std::vector<PolicyNamespace> namespaces,
        RetrieveCallback callback)
    : account_type_(account_type),
      account_id_(std::move(account_id)),
      namespaces_(std::move(namespaces)),
      callback_(std::move(callback)) {}

ComponentActiveDirectoryPolicyRetriever::
    ~ComponentActiveDirectoryPolicyRetriever() = default;

void ComponentActiveDirectoryPolicyRetriever::Start() {
  // Do a CHECK here, policy might be security relevant.
  CHECK(!start_was_called_);
  start_was_called_ = true;
  RetrievePolicyForNextNamespace();
}

void ComponentActiveDirectoryPolicyRetriever::RetrievePolicyForNextNamespace() {
  // Have we received all results yet?
  if (results_.size() == namespaces_.size()) {
    size_t total_response_count = namespaces_.size();
    int succeeded_count = total_response_count - failed_response_count_;
    VLOG(1) << "All retrieval calls finished (" << succeeded_count << "/"
            << total_response_count << " succeeded)";

    std::move(callback_).Run(std::move(results_));
    return;
  }

  // Request policy for the next namespace. Note that OnPolicyRetrieved()
  // calls back into this method to "peel off" the next namespace.
  DCHECK(results_.size() < namespaces_.size());
  const PolicyNamespace& ns = namespaces_[results_.size()];
  login_manager::PolicyDescriptor descriptor;
  descriptor.set_account_type(account_type_);
  descriptor.set_account_id(account_id_);
  descriptor.set_domain(MapPolicyDomain(ns.domain));
  descriptor.set_component_id(ns.component_id);

  chromeos::SessionManagerClient::Get()->RetrievePolicy(
      descriptor,
      base::BindOnce(
          &ComponentActiveDirectoryPolicyRetriever::OnPolicyRetrieved,
          weak_ptr_factory_.GetWeakPtr()));
}

void ComponentActiveDirectoryPolicyRetriever::OnPolicyRetrieved(
    ResponseType response,
    const std::string& policy_fetch_response_blob) {
  DCHECK(results_.size() < namespaces_.size());
  const PolicyNamespace& ns = namespaces_[results_.size()];
  results_.push_back({ns, response, policy_fetch_response_blob});

  if (response != ResponseType::SUCCESS) {
    failed_response_count_++;
    LOG(ERROR) << "Failed to retrieve policy for namespace "
               << "(" << ns.domain << "," << ns.component_id << ")"
               << ". Response: " << static_cast<int>(response);
  }

  RetrievePolicyForNextNamespace();
}

}  // namespace policy
