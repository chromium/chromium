// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/json/json_reader.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/schema_registry_service.h"
#include "chrome/browser/profiles/profile_impl.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/policy/core/common/policy_loader_lacros.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

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

class LacrosServiceObserverForTest : public chromeos::LacrosService::Observer {
 public:
  LacrosServiceObserverForTest(base::OnceClosure run_closure,
                               bool skip_initial_notification) {
    run_closure_ = std::move(run_closure);
    skip_initial_notification_ = skip_initial_notification;
  }

  void OnComponentPolicyUpdated(
      const policy::ComponentPolicyMap& policy) override {
    if (skip_initial_notification_) {
      skip_initial_notification_ = false;
      return;
    }
    std::move(run_closure_).Run();
  }

  base::OnceClosure run_closure_;
  bool skip_initial_notification_;
};

class ComponentPolicyLacrosBrowserTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    base::JSONReader::ValueWithError value_with_error =
        base::JSONReader::ReadAndReturnValueWithError(
            std::string(kTestPolicy1),
            base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);
    ASSERT_TRUE(value_with_error.value);
    base::Value json = std::move(value_with_error.value.value());
    ASSERT_TRUE(json.is_dict());

    policy::ComponentPolicyMap component_policy;
    component_policy[ns] = std::move(json);
    crosapi::mojom::BrowserInitParamsPtr params =
        crosapi::mojom::BrowserInitParams::New();
    params->device_account_component_policy = std::move(component_policy);
    chromeos::BrowserInitParams::SetInitParamsForTests(std::move(params));
    InProcessBrowserTest::SetUp();
  }

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

  std::unique_ptr<LacrosServiceObserverForTest> service_for_test_;
};

// Test to check the initial component policy received from Ash.
IN_PROC_BROWSER_TEST_F(ComponentPolicyLacrosBrowserTest,
                       BasicInitParamsSuccess) {
  auto* profile = ProfileManager::GetPrimaryUserProfile();

  auto* loader = g_browser_process->browser_policy_connector()
                     ->device_account_policy_loader();
  // The per_profile:False loader should have no component policy.
  ASSERT_FALSE(loader->component_policy());

  auto* registry = profile->GetPolicySchemaRegistryService()->registry();
  std::string error;
  registry->RegisterComponent(
      ns, policy::Schema::Parse(kValidationSchemaJson, &error));
  ASSERT_TRUE(error.empty());
  registry->SetDomainReady(ns.domain);
  base::RunLoop run_loop;
  service_for_test_ = std::make_unique<LacrosServiceObserverForTest>(
      run_loop.QuitClosure(), false);
  chromeos::LacrosService::Get()->AddObserver(service_for_test_.get());
  run_loop.RunUntilIdle();
  VerifyMap(map_size_test1, list_size_test1);
  chromeos::LacrosService::Get()->RemoveObserver(service_for_test_.get());
}

// Test to check the initial component policy received from Ash.
IN_PROC_BROWSER_TEST_F(ComponentPolicyLacrosBrowserTest, BasicUpdateSuccess) {
  auto* profile = ProfileManager::GetPrimaryUserProfile();

  auto* registry = profile->GetPolicySchemaRegistryService()->registry();
  std::string error;
  registry->RegisterComponent(
      ns, policy::Schema::Parse(kValidationSchemaJson, &error));
  ASSERT_TRUE(error.empty());
  registry->SetDomainReady(ns.domain);
  auto* lacros_service = chromeos::LacrosService::Get();

  base::RunLoop run_loop;
  service_for_test_ = std::make_unique<LacrosServiceObserverForTest>(
      run_loop.QuitClosure(), /*skip_initial_notification=*/true);
  lacros_service->AddObserver(service_for_test_.get());

  policy::ComponentPolicyMap component_policy;
  base::JSONReader::ValueWithError value_with_error =
      base::JSONReader::ReadAndReturnValueWithError(
          std::string(kTestPolicy2),
          base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);
  ASSERT_TRUE(value_with_error.value);
  base::Value json = std::move(value_with_error.value.value());
  ASSERT_TRUE(json.is_dict());
  component_policy[ns] = std::move(json);

  lacros_service->NotifyComponentPolicyUpdated(std::move(component_policy));
  run_loop.RunUntilIdle();
  VerifyMap(map_size_test2, list_size_test2);
  chromeos::LacrosService::Get()->RemoveObserver(service_for_test_.get());
}
