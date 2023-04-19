// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_PROFILE_POLICY_CONNECTOR_BUILDER_H_
#define CHROME_BROWSER_POLICY_PROFILE_POLICY_CONNECTOR_BUILDER_H_

#include <memory>

namespace user_manager {
class User;
}
namespace content {
class BrowserContext;
}

namespace policy {

class ChromeBrowserPolicyConnector;
class CloudPolicyStore;
class ConfigurationPolicyProvider;
class ProfilePolicyConnector;
class SchemaRegistry;
class CloudPolicyManager;

// Factory method that creates and initializes a new instance of
// ProfilePolicyConnector for the given |context|.
std::unique_ptr<ProfilePolicyConnector>
CreateProfilePolicyConnectorForBrowserContext(
    SchemaRegistry* schema_registry,
    CloudPolicyManager* cloud_policy_manager,
    ConfigurationPolicyProvider* user_policy_provider,
    policy::ChromeBrowserPolicyConnector* browser_policy_connector,
    bool force_immediate_load,
    content::BrowserContext* context);

// Factory method that creates and initializes a ProfilePolicyConnector.
std::unique_ptr<ProfilePolicyConnector> CreateAndInitProfilePolicyConnector(
    SchemaRegistry* schema_registry,
    policy::ChromeBrowserPolicyConnector* browser_policy_connector,
    ConfigurationPolicyProvider* policy_provider,
    const CloudPolicyStore* policy_store,
    bool force_immediate_load,
    const user_manager::User* user = nullptr);

// The next caller to create a ProfilePolicyConnector will get a PolicyService
// with |provider| as its sole policy provider. This can be called multiple
// times to override the policy providers for more than one Profile.
void PushProfilePolicyConnectorProviderForTesting(
    ConfigurationPolicyProvider* provider);

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_PROFILE_POLICY_CONNECTOR_BUILDER_H_
