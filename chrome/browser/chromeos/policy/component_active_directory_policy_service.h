// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_COMPONENT_ACTIVE_DIRECTORY_POLICY_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_COMPONENT_ACTIVE_DIRECTORY_POLICY_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/chromeos/policy/component_active_directory_policy_retriever.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema_registry.h"

namespace policy {

class PolicyBundle;

// Manages Active Directory policy for components (currently used for policy for
// extensions)
class ComponentActiveDirectoryPolicyService : public SchemaRegistry::Observer {
 public:
  class Delegate {
   public:
    virtual ~Delegate();

    virtual void OnComponentActiveDirectoryPolicyUpdated() = 0;
  };

  // |scope| specifies whether the component policy is fetched along with user
  // or device policy.
  //
  // |domain| specifies the domain for which policy is loaded. Must be
  // POLICY_DOMAIN_EXTENSIONS or POLICY_DOMAIN_SIGNIN_EXTENSIONS.
  //
  // |account_type| and |account_id| specify the Session Manager store from
  // which policy is retrieved. |account_id| must be the user's cryptohome id
  // for ACCOUNT_TYPE_USER and empty for ACCOUNT_TYPE_DEVICE. Not suppored yet
  // are ACCOUNT_TYPE_SESSIONLESS_USER and ACCOUNT_TYPE_DEVICE_LOCAL_ACCOUNT.
  //
  // Usually, |scope|, |domain|, |account_type| and |account_id| are coupled:
  // For user-level policy, use:
  //   POLICY_SCOPE_USER,
  //   POLICY_DOMAIN_EXTENSIONS,
  //   ACCOUNT_TYPE_USER,
  //   <user's cryptohome id>
  // For device-level policy, use:
  //   POLICY_SCOPE_DEVICE
  //   POLICY_DOMAIN_SIGNIN_EXTENSIONS
  //   ACCOUNT_TYPE_DEVICE
  //   <empty string>
  //
  // |delegate| is the object that gets notified when new policy is available.
  //
  // |schema_registry| contains the schemas used to validate extension policy.
  ComponentActiveDirectoryPolicyService(
      PolicyScope scope,
      PolicyDomain domain,
      login_manager::PolicyAccountType account_type,
      const std::string& account_id,
      Delegate* delegate,
      SchemaRegistry* schema_registry);
  ~ComponentActiveDirectoryPolicyService() override;

  // Retrieves policies from Session Manager, validates schemas and signals
  // OnComponentActiveDirectoryPolicyUpdated() in the |delegate_|. Does not
  // cancel in-flight retrieval request, but schedules a new retrieval request
  // when the current one finishes.
  void RetrievePolicies();

  // Returns the current policies for components or nullptr if the policies have
  // not been loaded yet.
  PolicyBundle* policy() const { return policy_.get(); }

 private:
  // SchemaRegistry::Observer implementation:
  void OnSchemaRegistryReady() override;
  void OnSchemaRegistryUpdated(bool has_new_schemas) override;

  // Called when the schema registry changed.
  void UpdateFromSchemaRegistry();

  // Called when policies have been retrieved from SessionManager. Parses the
  // JSON policy strings and calls FilterAndInstallPolicy().
  void OnPolicyRetrieved(
      std::vector<ComponentActiveDirectoryPolicyRetriever::RetrieveResult>
          results);

  // Does a schema validation of |policy| using |current_schema_map_|, then
  // sends the policy off to |delegate_|, where it's installed.
  void FilterAndInstallPolicy(std::unique_ptr<PolicyBundle> policy);

  // Constructor parameters.
  const PolicyScope scope_;
  const PolicyDomain domain_;
  const login_manager::PolicyAccountType account_type_;
  const std::string account_id_;
  Delegate* const delegate_;
  SchemaRegistry* const schema_registry_;

  // Schema map from the |schema_registry_|. Set as soon as the registry is
  // ready.
  scoped_refptr<SchemaMap> current_schema_map_;

  // Contains component policy for all namespaces in |domain_| that have a
  // registered schema. Set by RefreshPolicies() if the schema registry is
  // ready.
  std::unique_ptr<PolicyBundle> policy_;

  // Tracks policy retrieval requests to Session Manager.
  std::unique_ptr<ComponentActiveDirectoryPolicyRetriever> policy_retriever_;

  // If RetrievePolicies() is called while a requests is in-flight, the request
  // is deferred until the current one finishes. This flag keeps track of that.
  bool should_retrieve_again_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ComponentActiveDirectoryPolicyService> weak_ptr_factory_{
      this};
  DISALLOW_COPY_AND_ASSIGN(ComponentActiveDirectoryPolicyService);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_COMPONENT_ACTIVE_DIRECTORY_POLICY_SERVICE_H_
