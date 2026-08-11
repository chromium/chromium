// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/execution_context_priority/extension_service_worker_priority_voter.h"

#include <memory>
#include <string>

#include "base/strings/strcat.h"
#include "chrome/test/base/testing_profile.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/graph/worker_node_impl.h"
#include "components/performance_manager/public/execution_context/execution_context.h"
#include "components/performance_manager/public/render_process_host_proxy.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/voting.h"
#include "content/public/test/mock_render_process_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/mojom/api_permission_id.mojom-shared.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "extensions/common/permissions/permissions_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace performance_manager::execution_context_priority {

namespace {

using DummyVoteObserver = voting::test::DummyVoteObserver<Vote>;

constexpr char kExtensionId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

const execution_context::ExecutionContext* GetExecutionContext(
    const WorkerNode* worker_node) {
  return execution_context::ExecutionContext::From(worker_node);
}

class ExtensionServiceWorkerPriorityVoterTest : public GraphTestHarness {
 public:
  using Super = GraphTestHarness;

  ExtensionServiceWorkerPriorityVoterTest() = default;
  ~ExtensionServiceWorkerPriorityVoterTest() override = default;

  ExtensionServiceWorkerPriorityVoterTest(
      const ExtensionServiceWorkerPriorityVoterTest&) = delete;
  ExtensionServiceWorkerPriorityVoterTest& operator=(
      const ExtensionServiceWorkerPriorityVoterTest&) = delete;

  void SetUp() override {
    Super::SetUp();
    profile_ = std::make_unique<TestingProfile>();
    render_process_host_ =
        std::make_unique<content::MockRenderProcessHost>(profile_.get());
    voter_.InitializeOnGraph(graph(), observer_.BuildVotingChannel());
  }

  void TearDown() override {
    voter_.TearDownOnGraph(graph());
    render_process_host_.reset();
    profile_.reset();
    Super::TearDown();
  }

 protected:
  // Registers an enabled extension with `kExtensionId`, optionally holding the
  // `webRequestBlocking` permission. Note that MV3 only grants
  // `webRequestBlocking` to policy-installed extensions, so the extension is
  // built with a policy location.
  void AddEnabledExtension(bool with_web_request_blocking) {
    extensions::ExtensionBuilder builder("test");
    builder.SetID(kExtensionId);
    builder.SetLocation(extensions::mojom::ManifestLocation::kExternalPolicy);
    if (with_web_request_blocking) {
      builder.AddAPIPermission("webRequestBlocking");
    }
    scoped_refptr<const extensions::Extension> extension = builder.Build();
    ASSERT_EQ(extension->permissions_data()->HasAPIPermission(
                  extensions::mojom::APIPermissionID::kWebRequestBlocking),
              with_web_request_blocking);
    extensions::ExtensionRegistry::Get(profile_.get())->AddEnabled(extension);
  }

  TestNodeWrapper<ProcessNodeImpl> CreateProcessNode() {
    return CreateRendererProcessNode(RenderProcessHostProxy::CreateForTesting(
        render_process_host_->GetID()));
  }

  TestNodeWrapper<WorkerNodeImpl> CreateWorkerNode(
      ProcessNodeImpl* process_node,
      WorkerNode::WorkerType worker_type,
      const GURL& url) {
    blink::WorkerToken worker_token =
        worker_type == WorkerNode::WorkerType::kService
            ? blink::WorkerToken(blink::ServiceWorkerToken())
            : blink::WorkerToken(blink::SharedWorkerToken());
    return CreateNode<WorkerNodeImpl>(worker_type, process_node,
                                      profile_->UniqueToken(), worker_token,
                                      url::Origin::Create(url));
  }

  bool HasBoostVote(const WorkerNode* worker_node) const {
    return observer_.HasVote(
        voter_.voter_id(), GetExecutionContext(worker_node),
        base::Process::Priority::kUserBlocking,
        ExtensionServiceWorkerPriorityVoter::kPriorityReason);
  }

  bool HasNeutralVote(const WorkerNode* worker_node) const {
    return observer_.HasVote(
        voter_.voter_id(), GetExecutionContext(worker_node),
        base::Process::Priority::kMinValue,
        ExtensionServiceWorkerPriorityVoter::kPriorityReason);
  }

  const DummyVoteObserver& observer() const { return observer_; }

 private:
  DummyVoteObserver observer_;
  ExtensionServiceWorkerPriorityVoter voter_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::MockRenderProcessHost> render_process_host_;
};

}  // namespace

// The service worker of an extension with `webRequestBlocking` is boosted, and
// the vote goes away when the worker does.
TEST_F(ExtensionServiceWorkerPriorityVoterTest, BlockingExtensionIsBoosted) {
  AddEnabledExtension(/*with_web_request_blocking=*/true);

  auto process = CreateProcessNode();
  auto worker = CreateWorkerNode(
      process.get(), WorkerNode::WorkerType::kService,
      GURL(base::StrCat({"chrome-extension://", kExtensionId, "/sw.js"})));

  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(HasBoostVote(worker.get()));

  worker.reset();

  EXPECT_EQ(observer().GetVoteCount(), 0u);
}

// An extension without `webRequestBlocking` is not boosted. This is the case
// that regressed performance metrics when every extension was boosted. See
// crbug.com/493556675.
TEST_F(ExtensionServiceWorkerPriorityVoterTest,
       NonBlockingExtensionIsNotBoosted) {
  AddEnabledExtension(/*with_web_request_blocking=*/false);

  auto process = CreateProcessNode();
  auto worker = CreateWorkerNode(
      process.get(), WorkerNode::WorkerType::kService,
      GURL(base::StrCat({"chrome-extension://", kExtensionId, "/sw.js"})));

  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(HasNeutralVote(worker.get()));
}

// An extension that isn't in the registry (e.g. already unloaded) is not
// boosted.
TEST_F(ExtensionServiceWorkerPriorityVoterTest, UnknownExtensionIsNotBoosted) {
  auto process = CreateProcessNode();
  auto worker = CreateWorkerNode(
      process.get(), WorkerNode::WorkerType::kService,
      GURL(base::StrCat({"chrome-extension://", kExtensionId, "/sw.js"})));

  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(HasNeutralVote(worker.get()));
}

// Web service workers are never boosted, regardless of any installed
// extension.
TEST_F(ExtensionServiceWorkerPriorityVoterTest, WebServiceWorkerIsNotBoosted) {
  AddEnabledExtension(/*with_web_request_blocking=*/true);

  auto process = CreateProcessNode();
  auto worker =
      CreateWorkerNode(process.get(), WorkerNode::WorkerType::kService,
                       GURL("https://example.com/sw.js"));

  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(HasNeutralVote(worker.get()));
}

// Only service workers are boosted. Other extension workers don't gate
// navigations.
TEST_F(ExtensionServiceWorkerPriorityVoterTest,
       NonServiceExtensionWorkerIsNotBoosted) {
  AddEnabledExtension(/*with_web_request_blocking=*/true);

  auto process = CreateProcessNode();
  auto worker = CreateWorkerNode(
      process.get(), WorkerNode::WorkerType::kShared,
      GURL(base::StrCat({"chrome-extension://", kExtensionId, "/worker.js"})));

  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(HasNeutralVote(worker.get()));
}

}  // namespace performance_manager::execution_context_priority
