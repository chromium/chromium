// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/version_info/channel.h"
#include "chrome/browser/extensions/api/experimental_ai_data/experimental_ai_data_api.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {
namespace {

constexpr char kPolicyError[] = "Host access is restricted by policy.";

// Exercise the shared checks with a controllable asynchronous collection.
// Browser tests cover the production entry points and collection service.
class PolicyTestFunction : public ExperimentalAiDataGetAiDataFunction {
 public:
  raw_ptr<content::WebContents> contents = nullptr;
  AiDataKeyedService::BrowserData data;
  base::OnceClosure during_collection;
  bool collection_started = false;

 protected:
  ~PolicyTestFunction() override = default;

  ResponseAction Run() override {
    if (auto error = StartDataCollection(contents,
                                         /*max_tabs_for_text_collection=*/10)) {
      return RespondNow(Error(*error));
    }
    collection_started = true;
    if (during_collection) {
      std::move(during_collection).Run();
    }
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&PolicyTestFunction::OnDataCollected, this,
                                  std::move(data)));
    return RespondLater();
  }
};

class ExperimentalAiDataPolicyTest : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL("https://example.test/"));
    extension_ = ExtensionBuilder("AI Data Extension")
                     .SetID("hpkopmikdojpadgmioifjjodbmnjjjca")
                     .AddAPIPermission("experimentalAiData")
                     .Build();
  }

  scoped_refptr<PolicyTestFunction> CreateFunction() {
    auto function = base::MakeRefCounted<PolicyTestFunction>();
    function->set_extension(extension_);
    function->contents = web_contents();
    function->data.mutable_page_context()->set_url("https://example.test/");
    return function;
  }

  void BlockHost(const char* pattern = "*://example.test/*") {
    extension_->permissions_data()->SetPolicyHostRestrictions(
        URLPatternSet({URLPattern(URLPattern::SCHEME_ALL, pattern)}), {});
  }

  std::optional<base::Value> Run(PolicyTestFunction* function) {
    return api_test_utils::RunFunctionAndReturnSingleResult(function, "[]",
                                                            profile());
  }

  std::string RunError(PolicyTestFunction* function) {
    return api_test_utils::RunFunctionAndReturnError(function, "[]", profile());
  }

  ScopedCurrentChannel channel_{version_info::Channel::DEV};
  scoped_refptr<const Extension> extension_;
};

TEST_F(ExperimentalAiDataPolicyTest, AllowsHostsOutsidePolicyBlocklist) {
  BlockHost("*://unrelated.test/*");
  auto function = CreateFunction();
  EXPECT_TRUE(Run(function.get())) << function->GetError();
}

TEST_F(ExperimentalAiDataPolicyTest, RejectsIncognitoCallerAndTarget) {
  Profile* otr_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  auto caller_fn = CreateFunction();
  EXPECT_EQ("Incognito profile not supported.",
            api_test_utils::RunFunctionAndReturnError(
                caller_fn.get(), "[]", otr_profile,
                api_test_utils::FunctionMode::kIncognito));
  EXPECT_FALSE(caller_fn->collection_started);

  auto otr_contents = content::WebContents::Create(
      content::WebContents::CreateParams(otr_profile));
  auto target_fn = CreateFunction();
  target_fn->contents = otr_contents.get();
  EXPECT_EQ("Incognito profile not supported.",
            api_test_utils::RunFunctionAndReturnError(
                target_fn.get(), "[]", profile(),
                api_test_utils::FunctionMode::kIncognito));
  EXPECT_FALSE(target_fn->collection_started);
}

TEST_F(ExperimentalAiDataPolicyTest, RejectsBlockedNestedFrame) {
  auto* child =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("child");
  auto* nested =
      content::RenderFrameHostTester::For(child)->AppendChild("nested");
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://blocked.test/"), nested);
  BlockHost("*://blocked.test/*");
  auto function = CreateFunction();
  EXPECT_EQ(kPolicyError, RunError(function.get()));
  EXPECT_FALSE(function->collection_started);
}

TEST_F(ExperimentalAiDataPolicyTest, RejectsBlockedInheritedOrigin) {
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("about:blank"), main_rfh());
  ASSERT_EQ("https://example.test/",
            main_rfh()->GetLastCommittedOrigin().GetURL());
  BlockHost();
  auto function = CreateFunction();
  EXPECT_EQ(kPolicyError, RunError(function.get()));
  EXPECT_FALSE(function->collection_started);
}

TEST_F(ExperimentalAiDataPolicyTest, RejectsBlockedOpaquePrecursor) {
  // A data: child has no host in its URL, but retains its parent's origin as
  // its precursor. Making the origin opaque must not bypass the host policy.
  auto* child =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("child");
  child = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://blocked.test/"), child);
  child = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("data:text/html,protected content"), child);
  const url::Origin& origin = child->GetLastCommittedOrigin();
  ASSERT_TRUE(origin.opaque());
  ASSERT_EQ("https://blocked.test/",
            origin.GetTupleOrPrecursorTupleIfOpaque().GetURL());
  BlockHost("*://blocked.test/*");
  auto function = CreateFunction();
  EXPECT_EQ(kPolicyError, RunError(function.get()));
  EXPECT_FALSE(function->collection_started);
}

TEST_F(ExperimentalAiDataPolicyTest, RejectsBlockedEmbeddedContents) {
  auto inner =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  content::WebContentsTester::For(inner.get())
      ->NavigateAndCommit(GURL("https://blocked.test/"));
  auto* child =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("guest");
  web_contents()->AttachInnerWebContents(std::move(inner), child, false);
  BlockHost("*://blocked.test/*");
  auto function = CreateFunction();
  EXPECT_EQ(kPolicyError, RunError(function.get()));
  EXPECT_FALSE(function->collection_started);
}

TEST_F(ExperimentalAiDataPolicyTest, RechecksPolicyBeforeReturningData) {
  auto function = CreateFunction();
  function->during_collection =
      base::BindLambdaForTesting([&] { BlockHost(); });
  EXPECT_EQ(kPolicyError, RunError(function.get()));
  EXPECT_TRUE(function->collection_started);
}

TEST_F(ExperimentalAiDataPolicyTest, AllowsUnrelatedPolicyChanges) {
  auto function = CreateFunction();
  function->during_collection =
      base::BindLambdaForTesting([&] { BlockHost("*://unrelated.test/*"); });
  EXPECT_TRUE(Run(function.get())) << function->GetError();
}

TEST_F(ExperimentalAiDataPolicyTest, RejectsBlockedChildNavigation) {
  BlockHost("*://blocked.test/*");
  auto* child =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("child");
  auto function = CreateFunction();
  function->during_collection = base::BindLambdaForTesting([&] {
    content::NavigationSimulator::NavigateAndCommitFromDocument(
        GURL("https://blocked.test/"), child);
  });
  EXPECT_EQ(kPolicyError, RunError(function.get()));
}

TEST_F(ExperimentalAiDataPolicyTest, RejectsPageReplacement) {
  auto function = CreateFunction();
  function->during_collection = base::BindLambdaForTesting(
      [&] { NavigateAndCommit(GURL("https://other.test/")); });
  EXPECT_EQ("Page changed during data collection.", RunError(function.get()));
}

TEST_F(ExperimentalAiDataPolicyTest, RejectsTabClosure) {
  auto function = CreateFunction();
  function->during_collection = base::BindLambdaForTesting([&] {
    function->contents = nullptr;
    DeleteContents();
  });
  EXPECT_EQ("Page changed during data collection.", RunError(function.get()));
}

TEST_F(ExperimentalAiDataPolicyTest, ChecksAncillaryDataAndAllowedExceptions) {
  for (bool allowed_exception : {false, true}) {
    SCOPED_TRACE(allowed_exception);
    extension_->permissions_data()->SetPolicyHostRestrictions(
        URLPatternSet(
            {URLPattern(URLPattern::SCHEME_ALL, "*://blocked.test/*")}),
        allowed_exception
            ? URLPatternSet(
                  {URLPattern(URLPattern::SCHEME_ALL, "*://blocked.test/*")})
            : URLPatternSet());
    auto function = CreateFunction();
    auto* tab = function->data.add_tabs();
    tab->set_url("https://blocked.test/");
    tab->mutable_page_context()->set_inner_text("Protected page text");
    if (allowed_exception) {
      EXPECT_TRUE(Run(function.get())) << function->GetError();
    } else {
      EXPECT_EQ(kPolicyError, RunError(function.get()));
    }
  }
}

TEST_F(ExperimentalAiDataPolicyTest, AllowsBlockedSiteEngagementMetadata) {
  BlockHost("*://blocked.test/*");
  auto function = CreateFunction();
  // Engagement scores describe browsing activity without exposing page text.
  function->data.mutable_site_engagement()->add_entries()->set_url(
      "https://blocked.test/");
  EXPECT_TRUE(Run(function.get())) << function->GetError();
}

TEST_F(ExperimentalAiDataPolicyTest, AllowsBlockedTabMetadata) {
  BlockHost("*://blocked.test/*");
  auto function = CreateFunction();
  // URLs and titles alone do not expose the contents of the blocked page.
  function->data.add_tabs()->set_url("https://blocked.test/");
  EXPECT_TRUE(Run(function.get())) << function->GetError();
}

}  // namespace
}  // namespace extensions
