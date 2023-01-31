// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/repeating_test_future.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/schema_registry_service.h"
#include "chrome/browser/profiles/profile_impl.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/startup/browser_init_params.h"
#include "components/policy/core/common/policy_loader_lacros.h"
#include "components/policy/core/common/policy_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

const char kTestExtension[] = "super_secret_component_id";
constexpr char kValidationSchemaJson[] = R"(
{
  "type": "object",
  "properties": {
    "force_allowed_client_app_ids": {
      "title": "Force allowed client App identifiers.",
      "description": "List of client App identifiers",
      "type": "array",
      "items": {
        "type": "string",
        "minLength": 32,
        "maxLength": 32
      }
    }
  }
}
)";

constexpr char kTestPolicy1[] = R"(
    {"force_allowed_client_app_ids":
    {"Value":["haeblkpifdemlfnkogkipmghfcbonief"]}})";
constexpr char kTestPolicy2[] = R"(
    {"force_allowed_client_app_ids":{"Value":
    ["haeblkpifdemlfnkogkipmghfcbonie1","haeblkpifdemlfnkogkipmghfcbonie2"]}})";

int map_size_test1 = 1, map_size_test2 = 1, list_size_test1 = 1,
    list_size_test2 = 2;

const policy::PolicyNamespace ns(policy::POLICY_DOMAIN_EXTENSIONS,
                                 kTestExtension);

void VerifyMap(int map_size, int list_size) {
  auto* profile = ProfileManager::GetPrimaryUserProfile();
  auto* policy_connector = profile->GetProfilePolicyConnector();

  const policy::PolicyMap& map =
      policy_connector->policy_service()->GetPolicies(ns);
  EXPECT_THAT(map, testing::SizeIs(map_size));
  const policy::PolicyMap::Entry* entry =
      map.Get("force_allowed_client_app_ids");
  ASSERT_TRUE(entry);
  const base::Value* value = entry->value(base::Value::Type::LIST);
  ASSERT_TRUE(value);
  const base::Value::List& list = value->GetList();
  EXPECT_THAT(list, testing::SizeIs(list_size));
}

}  // namespace

class TestPolicyServiceObserver : public policy::PolicyService::Observer {
 public:
  TestPolicyServiceObserver(policy::PolicyService* policy_service,
                            policy::PolicyDomain policy_domain)
      : policy_service_(policy_service), policy_domain_(policy_domain) {
    policy_service_->AddObserver(policy_domain_, this);
  }

  ~TestPolicyServiceObserver() override {
    policy_service_->RemoveObserver(policy_domain_, this);
  }

  void OnPolicyUpdated(const policy::PolicyNamespace& nsp,
                       const policy::PolicyMap& previous,
                       const policy::PolicyMap& current) override {
    policy_updated_future_.AddValue(true);
  }

  void WaitForUpdate() { policy_updated_future_.Take(); }

  const raw_ptr<policy::PolicyService> policy_service_;
  const policy::PolicyDomain policy_domain_;

  base::test::RepeatingTestFuture<bool> policy_updated_future_;
};

class ComponentPolicyLacrosBrowserTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    base::Value json = base::test::ParseJson(kTestPolicy1);
    ASSERT_TRUE(json.is_dict());

    policy::ComponentPolicyMap component_policy;
    component_policy[ns] = std::move(json);
    crosapi::mojom::BrowserInitParamsPtr params =
        crosapi::mojom::BrowserInitParams::New();
    params->device_account_component_policy = std::move(component_policy);
    chromeos::BrowserInitParams::SetInitParamsForTests(std::move(params));
    InProcessBrowserTest::SetUp();
  }
};

// Test to check the initial component policy received from Ash.
IN_PROC_BROWSER_TEST_F(ComponentPolicyLacrosBrowserTest,
                       BasicInitParamsSuccess) {
  auto* profile = ProfileManager::GetPrimaryUserProfile();

  auto* registry = profile->GetPolicySchemaRegistryService()->registry();
  std::string error;
  registry->RegisterComponent(
      ns, policy::Schema::Parse(kValidationSchemaJson, &error));
  ASSERT_TRUE(error.empty());

  TestPolicyServiceObserver observer(
      profile->GetProfilePolicyConnector()->policy_service(), ns.domain);
  registry->SetDomainReady(ns.domain);
  observer.WaitForUpdate();
  VerifyMap(map_size_test1, list_size_test1);
}

// Test to check the update of component policy received from Ash.
IN_PROC_BROWSER_TEST_F(ComponentPolicyLacrosBrowserTest, BasicUpdateSuccess) {
  auto* profile = ProfileManager::GetPrimaryUserProfile();

  auto* registry = profile->GetPolicySchemaRegistryService()->registry();
  std::string error;
  registry->RegisterComponent(
      ns, policy::Schema::Parse(kValidationSchemaJson, &error));
  ASSERT_TRUE(error.empty());
  registry->SetDomainReady(ns.domain);

  TestPolicyServiceObserver observer(
      profile->GetProfilePolicyConnector()->policy_service(), ns.domain);
  policy::ComponentPolicyMap component_policy;

  base::Value json = base::test::ParseJson(kTestPolicy2);
  ASSERT_TRUE(json.is_dict());
  component_policy[ns] = std::move(json);

  chromeos::LacrosService::Get()->NotifyComponentPolicyUpdated(
      std::move(component_policy));
  observer.WaitForUpdate();
  VerifyMap(map_size_test2, list_size_test2);
}
