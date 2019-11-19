// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/component_active_directory_policy_service.h"

#include "base/bind.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/policy_builder.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {
namespace {

constexpr char kTestExtensionId[] = "abcdefghabcdefghabcdefghabcdefgh";
constexpr char kTestExtensionId2[] = "aaaabbbbccccddddeeeeffffgggghhhh";
constexpr char kTestUserAccountId[] = "test_user";
constexpr char kTestUserAccountId2[] = "test_user2";

constexpr char kTestPolicy[] = R"({
  "Policy": {
    "Name": "disabled"
  },
  "Recommended": {
    "Second": "maybe"
  }
})";

constexpr char kAltCapsTestPolicy[] = R"({
  "pOLIcY": {
    "Name": "disabled"
  },
  "rECOmmENDED": {
    "Second": "maybe"
  }
})";

constexpr char kTrailingCommaTestPolicy[] = R"({
  "Policy": {
    "Name": "disabled",
  },
  "Recommended": {
    "Second": "maybe",
  },
})";

constexpr char kInvalidTestPolicy[] = R"({
  "Policy": {
    "Name": "published",
    "Undeclared Name": "not published"
  }
})";

constexpr char kTestSchema[] = R"({
  "type": "object",
  "properties": {
    "Name": { "type": "string" },
    "Second": { "type": "string" }
  }
})";

constexpr char kTypeConversionTestPolicy[] = R"({
  "Policy": {
    "BooleanAsString": "1",
    "BooleanAsInteger": 1,
    "IntegerAsString": "1",
    "NumberAsString": "1.5",
    "NumberAsInteger": "1",
    "ListAsSubkeys": { "1":"One", "2":"Two", "NonNumeric":"Ignore" },
    "DictionaryAsJsonString" : "{\"Key\": \"Value\"}"
  }
})";

constexpr char kTypeConversionTestSchema[] = R"({
  "type": "object",
  "properties": {
    "BooleanAsString": { "type": "boolean" },
    "BooleanAsInteger": { "type": "boolean" },
    "IntegerAsString": { "type": "integer" },
    "NumberAsString": { "type": "number" },
    "NumberAsInteger": { "type": "number" },
    "ListAsSubkeys": { "type": "array", "items": { "type": "string" } },
    "DictionaryAsJsonString" : {
      "type": "object", "properties": { "Key": { "type": "string" } }
    }
  }
})";

class MockComponentActiveDirectoryPolicyDelegate
    : public ComponentActiveDirectoryPolicyService::Delegate {
 public:
  virtual ~MockComponentActiveDirectoryPolicyDelegate() {}

  MOCK_METHOD0(OnComponentActiveDirectoryPolicyUpdated, void());
};

}  // namespace

class ComponentActiveDirectoryPolicyServiceTest : public testing::Test {
 protected:
  ComponentActiveDirectoryPolicyServiceTest() {
    builder_.policy_data().set_policy_type(
        dm_protocol::kChromeExtensionPolicyType);
    builder_.policy_data().set_settings_entity_id(kTestExtensionId);

    expected_policy_.Set("Name", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                         POLICY_SOURCE_ACTIVE_DIRECTORY,
                         std::make_unique<base::Value>("disabled"), nullptr);
    expected_policy_.Set("Second", POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER,
                         POLICY_SOURCE_ACTIVE_DIRECTORY,
                         std::make_unique<base::Value>("maybe"), nullptr);

    chromeos::SessionManagerClient::InitializeFakeInMemory();

    SetPolicy(kTestPolicy);
    SetSchema(kTestSchema);

    service_ = std::make_unique<ComponentActiveDirectoryPolicyService>(
        POLICY_SCOPE_USER, POLICY_DOMAIN_EXTENSIONS,
        login_manager::ACCOUNT_TYPE_USER, kTestUserAccountId, &delegate_,
        &registry_);
  }

  ~ComponentActiveDirectoryPolicyServiceTest() override {
    chromeos::SessionManagerClient::Shutdown();
    // Make sure all StorePolicy() calls succeeded.
    EXPECT_EQ(store_policy_call_count_, store_policy_succeeded_count_);
  }

  void SetPolicy(std::string policy) {
    builder_.set_payload(std::move(policy));
  }

  void SetSchema(std::string schema) { curr_schema_ = std::move(schema); }

  void InitializeRegistry(const PolicyNamespace& ns) {
    // Create schema from string.
    DCHECK(!curr_schema_.empty());
    std::string error;
    Schema schema = Schema::Parse(curr_schema_, &error);
    EXPECT_TRUE(schema.valid()) << error;

    // Register schema.
    EXPECT_CALL(delegate_, OnComponentActiveDirectoryPolicyUpdated());
    registry_.RegisterComponent(ns, schema);
    registry_.SetAllDomainsReady();
    task_environment_.RunUntilIdle();
    testing::Mock::VerifyAndClearExpectations(&delegate_);
  }

  void StorePolicy(const char* account_id,
                   login_manager::PolicyDomain domain,
                   const std::string& component_id) {
    login_manager::PolicyDescriptor descriptor;
    descriptor.set_account_type(login_manager::ACCOUNT_TYPE_USER);
    descriptor.set_account_id(account_id);
    descriptor.set_domain(domain);
    descriptor.set_component_id(component_id);

    builder_.Build();
    chromeos::SessionManagerClient::Get()->StorePolicy(
        descriptor, builder_.GetBlob(),
        base::BindOnce(
            &ComponentActiveDirectoryPolicyServiceTest::OnPolicyStored,
            weak_ptr_factory_.GetWeakPtr()));
    store_policy_call_count_++;
  }

  std::unique_ptr<PolicyBundle> ExpectedBundle() {
    auto expected_bundle = std::make_unique<PolicyBundle>();
    expected_bundle->Get(kTestExtensionNS).CopyFrom(expected_policy_);
    return expected_bundle;
  }

  void CheckPolicyIsEmpty() {
    ASSERT_TRUE(service_->policy());
    EXPECT_TRUE(service_->policy()->Equals(PolicyBundle()))
        << "\nACTUAL BUNDLE:\n"
        << *service_->policy();
  }

  void CheckPolicyMatches(const PolicyBundle& expected_bundle) {
    ASSERT_TRUE(service_->policy());
    EXPECT_TRUE(service_->policy()->Equals(expected_bundle))
        << "\nACTUAL BUNDLE:\n"
        << *service_->policy() << "\nEXPECTED BUNDLE:\n"
        << expected_bundle;
  }

  const PolicyNamespace kTestExtensionNS =
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, kTestExtensionId);
  const PolicyNamespace kTestExtensionNS2 =
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, kTestExtensionId2);

  base::test::TaskEnvironment task_environment_;
  PolicyMap expected_policy_;
  SchemaRegistry registry_;
  std::unique_ptr<ComponentActiveDirectoryPolicyService> service_;
  MockComponentActiveDirectoryPolicyDelegate delegate_;

 private:
  void OnPolicyStored(bool result) {
    if (result)
      store_policy_succeeded_count_++;
  }

  ComponentActiveDirectoryPolicyBuilder builder_;
  std::string curr_schema_;
  int store_policy_call_count_ = 0;
  int store_policy_succeeded_count_ = 0;
  base::WeakPtrFactory<ComponentActiveDirectoryPolicyServiceTest>
      weak_ptr_factory_{this};
};

// Before registry is ready, RetrievePolicies() should be a no-op.
TEST_F(ComponentActiveDirectoryPolicyServiceTest, PolicyNotSetWithoutRegistry) {
  EXPECT_CALL(delegate_, OnComponentActiveDirectoryPolicyUpdated()).Times(0);
  service_->RetrievePolicies();
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(service_->policy());
}

// SchemaRegistry::SetAllDomainsReady should trigger RetrievePolicies().
TEST_F(ComponentActiveDirectoryPolicyServiceTest, RegistryTriggersRetrieval) {
  EXPECT_CALL(delegate_, OnComponentActiveDirectoryPolicyUpdated());
  registry_.SetAllDomainsReady();
  task_environment_.RunUntilIdle();
  CheckPolicyIsEmpty();
}

// Check if RetrievePolicies() actually retrieves policy for registered schemas.
TEST_F(ComponentActiveDirectoryPolicyServiceTest, RetrievePolicies) {
  InitializeRegistry(kTestExtensionNS);

  // Storing policy won't trigger a policy update since policy is usually stored
  // by authpolicyd. The class relies on ActiveDirectoryPolicyManager calling
  // RetrievePolicies() explicitly.
  EXPECT_CALL(delegate_, OnComponentActiveDirectoryPolicyUpdated()).Times(0);
  StorePolicy(kTestUserAccountId, login_manager::POLICY_DOMAIN_EXTENSIONS,
              kTestExtensionId);
  task_environment_.RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&delegate_);

  // Calling RetrievePolicies() should get the policy now.
  EXPECT_CALL(delegate_, OnComponentActiveDirectoryPolicyUpdated());
  service_->RetrievePolicies();
  CheckPolicyIsEmpty();
  task_environment_.RunUntilIdle();
  CheckPolicyMatches(*ExpectedBundle());
}

// Once the registry is ready, a policy fetch should be triggered.
TEST_F(ComponentActiveDirectoryPolicyServiceTest,
       RetrievePoliciesTriggeredByRegistry) {
  StorePolicy(kTestUserAccountId, login_manager::POLICY_DOMAIN_EXTENSIONS,
              kTestExtensionId);
  InitializeRegistry(kTestExtensionNS);
  CheckPolicyMatches(*ExpectedBundle());
}

// Clearing the schema should also remove the policy for that extension.
TEST_F(ComponentActiveDirectoryPolicyServiceTest, ClearingSchemaRemovesPolicy) {
  StorePolicy(kTestUserAccountId, login_manager::POLICY_DOMAIN_EXTENSIONS,
              kTestExtensionId);
  InitializeRegistry(kTestExtensionNS);
  CheckPolicyMatches(*ExpectedBundle());

  // Unregistering should trigger RetrievePolicies().
  EXPECT_CALL(delegate_, OnComponentActiveDirectoryPolicyUpdated());
  registry_.UnregisterComponent(kTestExtensionNS);
  task_environment_.RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&delegate_);
  CheckPolicyIsEmpty();
}

// Trying to retrieve policy for another extension shouldn't work.
TEST_F(ComponentActiveDirectoryPolicyServiceTest, WontRetrieveWrongNamespace) {
  // Note: kTestExtensionId2 is the wrong extension id.
  StorePolicy(kTestUserAccountId, login_manager::POLICY_DOMAIN_EXTENSIONS,
              kTestExtensionId2);
  InitializeRegistry(kTestExtensionNS);
  CheckPolicyIsEmpty();
}

// Trying to retrieve policy from another domain shouldn't work.
TEST_F(ComponentActiveDirectoryPolicyServiceTest, WontRetrieveWrongDomain) {
  // Note: POLICY_DOMAIN_SIGNIN_EXTENSIONS is the wrong domain.
  StorePolicy(kTestUserAccountId,
              login_manager::POLICY_DOMAIN_SIGNIN_EXTENSIONS, kTestExtensionId);
  InitializeRegistry(kTestExtensionNS);
  CheckPolicyIsEmpty();
}

// Trying to retrieve policy for another user shouldn't work.
TEST_F(ComponentActiveDirectoryPolicyServiceTest, WontRetrieveWrongAccountId) {
  // Note: kTestUserAccountId2 is the wrong user.
  StorePolicy(kTestUserAccountId2, login_manager::POLICY_DOMAIN_EXTENSIONS,
              kTestExtensionId);
  InitializeRegistry(kTestExtensionNS);
  CheckPolicyIsEmpty();
}

// "Policy" and "Recommended" should be treated case insensitively.
// kAltCapsTestPolicy uses a different capitalization for those keys.
TEST_F(ComponentActiveDirectoryPolicyServiceTest,
       ComparesPolicyLevelCaseInsensitively) {
  SetPolicy(kAltCapsTestPolicy);
  StorePolicy(kTestUserAccountId, login_manager::POLICY_DOMAIN_EXTENSIONS,
              kTestExtensionId);
  InitializeRegistry(kTestExtensionNS);
  CheckPolicyMatches(*ExpectedBundle());
}

// JSON with tailing commas is allowed.
TEST_F(ComponentActiveDirectoryPolicyServiceTest, AcceptsTrailingCommas) {
  SetPolicy(kTrailingCommaTestPolicy);
  StorePolicy(kTestUserAccountId, login_manager::POLICY_DOMAIN_EXTENSIONS,
              kTestExtensionId);
  InitializeRegistry(kTestExtensionNS);
  CheckPolicyMatches(*ExpectedBundle());
}

// Tests a bunch of type conversions from registry to base::value, see
// ConvertRegistryValue().
TEST_F(ComponentActiveDirectoryPolicyServiceTest, ConvertsTypes) {
  SetPolicy(kTypeConversionTestPolicy);
  SetSchema(kTypeConversionTestSchema);

  expected_policy_.Clear();
  expected_policy_.Set("BooleanAsString", POLICY_LEVEL_MANDATORY,
                       POLICY_SCOPE_USER, POLICY_SOURCE_ACTIVE_DIRECTORY,
                       std::make_unique<base::Value>(true), nullptr);

  expected_policy_.Set("BooleanAsInteger", POLICY_LEVEL_MANDATORY,
                       POLICY_SCOPE_USER, POLICY_SOURCE_ACTIVE_DIRECTORY,
                       std::make_unique<base::Value>(true), nullptr);

  expected_policy_.Set("IntegerAsString", POLICY_LEVEL_MANDATORY,
                       POLICY_SCOPE_USER, POLICY_SOURCE_ACTIVE_DIRECTORY,
                       std::make_unique<base::Value>(1), nullptr);

  expected_policy_.Set("NumberAsString", POLICY_LEVEL_MANDATORY,
                       POLICY_SCOPE_USER, POLICY_SOURCE_ACTIVE_DIRECTORY,
                       std::make_unique<base::Value>(1.5), nullptr);

  expected_policy_.Set("NumberAsInteger", POLICY_LEVEL_MANDATORY,
                       POLICY_SCOPE_USER, POLICY_SOURCE_ACTIVE_DIRECTORY,
                       std::make_unique<base::Value>(1.0), nullptr);

  auto list = std::make_unique<base::ListValue>();
  list->Append(base::Value("One"));
  list->Append(base::Value("Two"));
  expected_policy_.Set("ListAsSubkeys", POLICY_LEVEL_MANDATORY,
                       POLICY_SCOPE_USER, POLICY_SOURCE_ACTIVE_DIRECTORY,
                       std::move(list), nullptr);

  auto dict = std::make_unique<base::DictionaryValue>();
  dict->SetKey("Key", base::Value("Value"));
  expected_policy_.Set("DictionaryAsJsonString", POLICY_LEVEL_MANDATORY,
                       POLICY_SCOPE_USER, POLICY_SOURCE_ACTIVE_DIRECTORY,
                       std::move(dict), nullptr);

  StorePolicy(kTestUserAccountId, login_manager::POLICY_DOMAIN_EXTENSIONS,
              kTestExtensionId);
  InitializeRegistry(kTestExtensionNS);
  CheckPolicyMatches(*ExpectedBundle());
}

// JSON values that are not in the schema should be ignored.
TEST_F(ComponentActiveDirectoryPolicyServiceTest, IgnoresValuesNotInSchema) {
  SetPolicy(kInvalidTestPolicy);

  expected_policy_.Clear();
  expected_policy_.Set("Name", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                       POLICY_SOURCE_ACTIVE_DIRECTORY,
                       std::make_unique<base::Value>("published"), nullptr);
  // "Undeclared Name" is expected to be missing since it's not in the schema.

  StorePolicy(kTestUserAccountId, login_manager::POLICY_DOMAIN_EXTENSIONS,
              kTestExtensionId);
  InitializeRegistry(kTestExtensionNS);
  CheckPolicyMatches(*ExpectedBundle());
}

// If RetrievePolicies() is called while a request is in-flight, it should be
// queued up and scheduled when the in-flight request is finished. If more calls
// happen in between, they should be eaten.
TEST_F(ComponentActiveDirectoryPolicyServiceTest,
       QueuesRetrievalIfOneIsInFlight) {
  // Register schema for both test extension namespaces.
  InitializeRegistry(kTestExtensionNS);
  InitializeRegistry(kTestExtensionNS2);

  // OnComponentActiveDirectoryPolicyUpdated should only be called twice,
  // although we scheduled 3 retrieve requests. The middle one should be eaten.
  EXPECT_CALL(delegate_, OnComponentActiveDirectoryPolicyUpdated()).Times(2);

  // Set policy for extension 1 and queue two retrieval requests.
  StorePolicy(kTestUserAccountId, login_manager::POLICY_DOMAIN_EXTENSIONS,
              kTestExtensionId);
  service_->RetrievePolicies();
  service_->RetrievePolicies();

  // Set (the same) policy for extension 2 and queue another retrieval request.
  StorePolicy(kTestUserAccountId, login_manager::POLICY_DOMAIN_EXTENSIONS,
              kTestExtensionId2);
  service_->RetrievePolicies();
  task_environment_.RunUntilIdle();

  // We should have received policy for both namespaces.
  PolicyBundle expected_bundle;
  expected_bundle.Get(kTestExtensionNS).CopyFrom(expected_policy_);
  expected_bundle.Get(kTestExtensionNS2).CopyFrom(expected_policy_);
  CheckPolicyMatches(expected_bundle);
}

}  // namespace policy
