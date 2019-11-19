// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/component_active_directory_policy_retriever.h"

#include "base/bind.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "components/policy/core/common/cloud/policy_builder.h"
#include "components/policy/core/common/policy_test_utils.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace {
constexpr char kExtensionId[] = "abcdefghabcdefghabcdefghabcdefgh";
constexpr char kFakePolicy[] = "fake_policy";
constexpr char kEmptyAccountId[] = "";

}  // namespace

namespace policy {

using RetrieveResult = ComponentActiveDirectoryPolicyRetriever::RetrieveResult;
using RetrieveCallback =
    ComponentActiveDirectoryPolicyRetriever::RetrieveCallback;
using ResponseType = ComponentActiveDirectoryPolicyRetriever::ResponseType;

class ComponentActiveDirectoryPolicyRetrieverTest : public testing::Test {
 protected:
  ComponentActiveDirectoryPolicyRetrieverTest() = default;

  void SetUp() override {
    chromeos::SessionManagerClient::InitializeFakeInMemory();
  }

  void TearDown() override { chromeos::SessionManagerClient::Shutdown(); }

  RetrieveCallback CreateRetrieveCallback() {
    return base::BindOnce(
        &ComponentActiveDirectoryPolicyRetrieverTest::OnPoliciesRetrieved,
        base::Unretained(this));
  }

  void OnPoliciesRetrieved(std::vector<RetrieveResult> results) {
    callback_called_ = true;
    results_ = std::move(results);
  }

  chromeos::VoidDBusMethodCallback CreateStoredCallback() {
    return base::BindOnce(
        &ComponentActiveDirectoryPolicyRetrieverTest::OnPolicyStored,
        base::Unretained(this));
  }

  void OnPolicyStored(bool success) { policy_stored_ = success; }

  base::test::TaskEnvironment task_environment_;
  std::vector<RetrieveResult> results_;
  bool policy_stored_ = false;
  bool callback_called_ = false;
};

TEST_F(ComponentActiveDirectoryPolicyRetrieverTest, RetrieveNoPolicy) {
  const std::vector<PolicyNamespace> empty_namespaces;
  EXPECT_FALSE(callback_called_);
  ComponentActiveDirectoryPolicyRetriever retriever(
      login_manager::ACCOUNT_TYPE_DEVICE, kEmptyAccountId, empty_namespaces,
      CreateRetrieveCallback());
  retriever.Start();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(results_.empty());
  EXPECT_TRUE(callback_called_);
}

TEST_F(ComponentActiveDirectoryPolicyRetrieverTest, RetrieveEmptyPolicy) {
  std::vector<PolicyNamespace> namespaces = {
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, kExtensionId)};
  ComponentActiveDirectoryPolicyRetriever retriever(
      login_manager::ACCOUNT_TYPE_DEVICE, kEmptyAccountId, namespaces,
      CreateRetrieveCallback());
  retriever.Start();
  task_environment_.RunUntilIdle();
  ASSERT_EQ(1UL, results_.size());
  EXPECT_EQ(namespaces[0], results_[0].ns);
  EXPECT_EQ(ResponseType::SUCCESS, results_[0].response);
  EXPECT_TRUE(results_[0].policy_fetch_response_blob.empty());
}

TEST_F(ComponentActiveDirectoryPolicyRetrieverTest, RetrievePolicies) {
  // Build a policy fetch response blob (|session_manager_client_| needs one).
  ComponentActiveDirectoryPolicyBuilder builder;
  builder.set_payload(kFakePolicy);
  builder.Build();
  const std::string policy_blob = builder.GetBlob();

  // Store the fake extension policy.
  login_manager::PolicyDescriptor descriptor;
  descriptor.set_account_type(login_manager::ACCOUNT_TYPE_DEVICE);
  descriptor.set_domain(login_manager::POLICY_DOMAIN_EXTENSIONS);
  descriptor.set_component_id(kExtensionId);
  chromeos::SessionManagerClient::Get()->StorePolicy(descriptor, policy_blob,
                                                     CreateStoredCallback());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(policy_stored_);

  // Retrieve the fake extension policy and make sure it matches.
  std::vector<PolicyNamespace> namespaces = {
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, kExtensionId)};
  ComponentActiveDirectoryPolicyRetriever retriever(
      login_manager::ACCOUNT_TYPE_DEVICE, kEmptyAccountId, namespaces,
      CreateRetrieveCallback());
  retriever.Start();
  task_environment_.RunUntilIdle();
  ASSERT_EQ(1UL, results_.size());
  EXPECT_EQ(namespaces[0], results_[0].ns);
  EXPECT_EQ(ResponseType::SUCCESS, results_[0].response);
  EXPECT_EQ(policy_blob, results_[0].policy_fetch_response_blob);
}

// Makes sure cancellation won't access invalid memory.
TEST_F(ComponentActiveDirectoryPolicyRetrieverTest, CancelClean) {
  std::vector<PolicyNamespace> namespaces = {
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, kExtensionId)};
  auto retriever = std::make_unique<ComponentActiveDirectoryPolicyRetriever>(
      login_manager::ACCOUNT_TYPE_DEVICE, kEmptyAccountId, namespaces,
      CreateRetrieveCallback());
  retriever->Start();
  retriever.reset();
  task_environment_.RunUntilIdle();
  ASSERT_EQ(0UL, results_.size());
}

}  // namespace policy
