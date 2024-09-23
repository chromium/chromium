// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_content/declarative_content_css_condition_tracker.h"

#include <memory>
#include <tuple>

#include "base/test/values_test_util.h"
#include "chrome/browser/extensions/api/declarative_content/content_predicate_evaluator.h"
#include "chrome/browser/extensions/api/declarative_content/declarative_content_condition_tracker_test.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/mock_render_process_host.h"
#include "extensions/browser/api/declarative/rules_registry_service.h"
#include "extensions/browser/api/declarative_content/content_rules_registry.h"
#include "extensions/browser/renderer_startup_helper.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/host_id.mojom.h"
#include "extensions/common/mojom/renderer.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions {

using testing::HasSubstr;
using testing::UnorderedElementsAre;
using testing::UnorderedElementsAreArray;

namespace {

// Class that implements the binding of a new Renderer mojom interface and
// can receive callbacks on it for testing validation.
class InterceptingRendererStartupHelper : public RendererStartupHelper,
                                          public mojom::Renderer {
 public:
  explicit InterceptingRendererStartupHelper(
      content::BrowserContext* browser_context)
      : RendererStartupHelper(browser_context) {}

  void Reset() { is_watch_pages_called_ = false; }
  void SetWatchPagesCalledClosure(base::OnceClosure on_watch_pages_done) {
    on_watch_pages_done_ = std::move(on_watch_pages_done);
  }

  bool IsWatchPagesCalled() { return is_watch_pages_called_; }
  const std::vector<std::string>& CssSelector() { return css_selectors_; }

 protected:
  mojo::PendingAssociatedRemote<mojom::Renderer> BindNewRendererRemote(
      content::RenderProcessHost* process) override {
    mojo::AssociatedRemote<mojom::Renderer> remote;
    receivers_.Add(this, remote.BindNewEndpointAndPassDedicatedReceiver());
    return remote.Unbind();
  }

 private:
  // mojom::Renderer implementation:
  void ActivateExtension(const ExtensionId& extension_id) override {}
  void SetActivityLoggingEnabled(bool enabled) override {}
  void LoadExtensions(
      std::vector<mojom::ExtensionLoadedParamsPtr> loaded_extensions) override {
  }
  void UnloadExtension(const ExtensionId& extension_id) override {}
  void SuspendExtension(
      const ExtensionId& extension_id,
      mojom::Renderer::SuspendExtensionCallback callback) override {
    std::move(callback).Run();
  }
  void CancelSuspendExtension(const ExtensionId& extension_id) override {}
  void SetDeveloperMode(bool current_developer_mode) override {}
  void SetSessionInfo(version_info::Channel channel,
                      mojom::FeatureSessionType session,
                      bool is_lock_screen_context) override {}
  void SetSystemFont(const std::string& font_family,
                     const std::string& font_size) override {}
  void SetWebViewPartitionID(const std::string& partition_id) override {}
  void SetScriptingAllowlist(
      const std::vector<ExtensionId>& extension_ids) override {}
  void UpdateUserScriptWorlds(
      std::vector<mojom::UserScriptWorldInfoPtr> info) override {}
  void ClearUserScriptWorldConfig(
      const std::string& extension_id,
      const std::optional<std::string>& world_id) override {}
  void ShouldSuspend(ShouldSuspendCallback callback) override {
    std::move(callback).Run();
  }
  void TransferBlobs(TransferBlobsCallback callback) override {
    std::move(callback).Run();
  }
  void UpdatePermissions(const ExtensionId& extension_id,
                         PermissionSet active_permissions,
                         PermissionSet withheld_permissions,
                         URLPatternSet policy_blocked_hosts,
                         URLPatternSet policy_allowed_hosts,
                         bool uses_default_policy_host_restrictions) override {}
  void UpdateDefaultPolicyHostRestrictions(
      URLPatternSet default_policy_blocked_hosts,
      URLPatternSet default_policy_allowed_hosts) override {}
  void UpdateUserHostRestrictions(URLPatternSet user_blocked_hosts,
                                  URLPatternSet user_allowed_hosts) override {}
  void UpdateTabSpecificPermissions(const ExtensionId& extension_id,
                                    URLPatternSet new_hosts,
                                    int tab_id,
                                    bool update_origin_allowlist) override {}
  void UpdateUserScripts(base::ReadOnlySharedMemoryRegion shared_memory,
                         mojom::HostIDPtr host_id) override {}
  void ClearTabSpecificPermissions(
      const std::vector<ExtensionId>& extension_ids,
      int tab_id,
      bool update_origin_allowlist) override {}
  void WatchPages(const std::vector<std::string>& css_selectors) override {
    is_watch_pages_called_ = true;
    css_selectors_ = css_selectors;
    std::move(on_watch_pages_done_).Run();
  }

  bool is_watch_pages_called_ = false;
  std::vector<std::string> css_selectors_;
  base::OnceClosure on_watch_pages_done_;
  mojo::AssociatedReceiverSet<mojom::Renderer> receivers_;
};

}  // namespace

class DeclarativeContentCssConditionTrackerTest
    : public DeclarativeContentConditionTrackerTest {
 public:
  DeclarativeContentCssConditionTrackerTest(
      const DeclarativeContentCssConditionTrackerTest&) = delete;
  DeclarativeContentCssConditionTrackerTest& operator=(
      const DeclarativeContentCssConditionTrackerTest&) = delete;

 protected:
  DeclarativeContentCssConditionTrackerTest() : tracker_(&delegate_) {}

  static std::unique_ptr<KeyedService> BuildFakeRendererStartupHelper(
      content::BrowserContext* context) {
    return std::make_unique<InterceptingRendererStartupHelper>(context);
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return {TestingProfile::TestingFactory{
        RendererStartupHelperFactory::GetInstance(),
        base::BindRepeating(&BuildFakeRendererStartupHelper)}};
  }

  void SimulateRenderProcessCreated(content::RenderProcessHost* rph) {
    RendererStartupHelperFactory::GetForBrowserContext(profile())
        ->OnRenderProcessHostCreated(rph);
  }

  class Delegate : public ContentPredicateEvaluator::Delegate {
   public:
    Delegate() : evaluation_requests_(0) {}

    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    int evaluation_requests() { return evaluation_requests_; }

    // ContentPredicateEvaluator::Delegate:
    void RequestEvaluation(content::WebContents* contents) override {
      ++evaluation_requests_;
    }

    bool ShouldManageConditionsForBrowserContext(
        content::BrowserContext* context) override {
      return true;
    }

   private:
    int evaluation_requests_;
  };

  // Creates a predicate with appropriate expectations of success.
  std::unique_ptr<const ContentPredicate> CreatePredicate(
      const std::string& value) {
    std::unique_ptr<const ContentPredicate> predicate;
    CreatePredicateImpl(value, &predicate);
    return predicate;
  }

  // Expects the WatchPages Mojo method is called with |selectors| as the param,
  // after invoking |func|.
  template <class Func>
  void ExpectWatchPagesMessage(content::WebContents* tab,
                               const std::set<std::string>& selectors,
                               const Func& func) {
    auto* helper = static_cast<InterceptingRendererStartupHelper*>(
        RendererStartupHelperFactory::GetForBrowserContext(profile()));

    helper->Reset();
    base::RunLoop run_loop;
    helper->SetWatchPagesCalledClosure(run_loop.QuitClosure());
    func();
    run_loop.Run();

    EXPECT_TRUE(helper->IsWatchPagesCalled());
    EXPECT_THAT(helper->CssSelector(), UnorderedElementsAreArray(selectors));
  }

  // Expects no the WatchPages Mojo method is called after invoking |func|.
  template <class Func>
  void ExpectNoWatchPagesMessage(content::WebContents* tab,
                                 const Func& func) {
    auto* helper = static_cast<InterceptingRendererStartupHelper*>(
        RendererStartupHelperFactory::GetForBrowserContext(profile()));
    helper->Reset();
    base::RunLoop run_loop;
    func();
    run_loop.RunUntilIdle();

    EXPECT_FALSE(helper->IsWatchPagesCalled());
  }

  // Calls OnWatchedPageChanged() with the tab to simulate the watched page
  // is changed.
  void SimulateWatchedPageChanged(content::WebContents* tab,
                                  const std::vector<std::string>& selectors) {
    tracker_.OnWatchedPageChanged(tab, selectors);
  }

  Delegate delegate_;
  DeclarativeContentCssConditionTracker tracker_;

 private:
  // This function exists to work around the gtest limitation that functions
  // with fatal assertions must return void.
  void CreatePredicateImpl(const std::string& value,
                           std::unique_ptr<const ContentPredicate>* predicate) {
    std::string error;
    *predicate =
        tracker_.CreatePredicate(nullptr, base::test::ParseJson(value), &error);
    EXPECT_EQ("", error);
    ASSERT_TRUE(*predicate);
  }
};

TEST(DeclarativeContentCssPredicateTest, WrongCssDatatype) {
  std::string error;
  std::unique_ptr<DeclarativeContentCssPredicate> predicate =
      DeclarativeContentCssPredicate::Create(nullptr, base::Value("selector"),
                                             &error);
  EXPECT_THAT(error, HasSubstr("invalid type"));
  EXPECT_FALSE(predicate);
}

TEST(DeclarativeContentCssPredicateTest, CssPredicate) {
  std::string error;
  std::unique_ptr<DeclarativeContentCssPredicate> predicate =
      DeclarativeContentCssPredicate::Create(
          nullptr, base::test::ParseJson("[\"input\", \"a\"]"), &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(predicate);

  EXPECT_THAT(predicate->css_selectors(), UnorderedElementsAre("input", "a"));
}

// Tests that adding and removing predicates causes a WatchPages message to be
// sent.
TEST_F(DeclarativeContentCssConditionTrackerTest, AddAndRemovePredicates) {
  const std::unique_ptr<content::WebContents> tab = MakeTab();
  SimulateRenderProcessCreated(GetMockRenderProcessHost(tab.get()));
  tracker_.TrackForWebContents(tab.get());
  EXPECT_EQ(0, delegate_.evaluation_requests());

  // Check that adding predicates sends a WatchPages message with the
  // corresponding selectors to the tab's RenderProcessHost.
  std::vector<std::unique_ptr<const ContentPredicate>> predicates;
  predicates.push_back(CreatePredicate("[\"a\", \"div\"]"));
  predicates.push_back(CreatePredicate("[\"b\"]"));
  predicates.push_back(CreatePredicate("[\"input\"]"));

  // Add the predicates in two groups: (0, 1) and (2).
  std::map<const void*, std::vector<const ContentPredicate*>> predicate_groups;
  const void* const group1 = GeneratePredicateGroupID();
  predicate_groups[group1].push_back(predicates[0].get());
  predicate_groups[group1].push_back(predicates[1].get());
  const void* const group2 = GeneratePredicateGroupID();
  predicate_groups[group2].push_back(predicates[2].get());

  std::set<std::string> watched_selectors;
  watched_selectors.insert("a");
  watched_selectors.insert("div");
  watched_selectors.insert("b");
  watched_selectors.insert("input");
  ExpectWatchPagesMessage(tab.get(), watched_selectors,
                          [this, &predicate_groups]() {
    tracker_.TrackPredicates(predicate_groups);
  });
  EXPECT_EQ(0, delegate_.evaluation_requests());

  // Remove the first group of predicates.
  watched_selectors.erase("a");
  watched_selectors.erase("div");
  watched_selectors.erase("b");
  ExpectWatchPagesMessage(tab.get(), watched_selectors, [this, group1]() {
    tracker_.StopTrackingPredicates(std::vector<const void*>(1, group1));
  });
  EXPECT_EQ(0, delegate_.evaluation_requests());

  // Remove the second group of predicates.
  watched_selectors.erase("input");
  ExpectWatchPagesMessage(tab.get(), watched_selectors, [this, &group2]() {
    tracker_.StopTrackingPredicates(std::vector<const void*>(1, group2));
  });
  EXPECT_EQ(0, delegate_.evaluation_requests());
}

// Tests that adding and removing predicates that specify the same CSS selector
// as an existing predicate does not cause a WatchPages message to be sent.
TEST_F(DeclarativeContentCssConditionTrackerTest,
       AddAndRemovePredicatesWithSameSelectors) {
  const std::unique_ptr<content::WebContents> tab = MakeTab();
  SimulateRenderProcessCreated(GetMockRenderProcessHost(tab.get()));
  tracker_.TrackForWebContents(tab.get());
  EXPECT_EQ(0, delegate_.evaluation_requests());

  // Add the first predicate and expect a WatchPages message.
  std::string error;
  std::unique_ptr<const ContentPredicate> predicate1 =
      CreatePredicate("[\"a\"]");

  std::map<const void*, std::vector<const ContentPredicate*>> predicate_groups1;
  const void* const group1 = GeneratePredicateGroupID();
  predicate_groups1[group1].push_back(predicate1.get());

  std::set<std::string> watched_selectors;
  watched_selectors.insert("a");
  ExpectWatchPagesMessage(tab.get(), watched_selectors,
                          [this, &predicate_groups1]() {
    tracker_.TrackPredicates(predicate_groups1);
  });
  EXPECT_EQ(0, delegate_.evaluation_requests());

  // Add the second predicate specifying the same selector and expect no
  // WatchPages message.
  std::unique_ptr<const ContentPredicate> predicate2 =
      CreatePredicate("[\"a\"]");

  std::map<const void*, std::vector<const ContentPredicate*>> predicate_groups2;
  const void* const group2 = GeneratePredicateGroupID();
  predicate_groups2[group2].push_back(predicate2.get());

  ExpectNoWatchPagesMessage(tab.get(), [this, &predicate_groups2]() {
    tracker_.TrackPredicates(predicate_groups2);
  });
  EXPECT_EQ(0, delegate_.evaluation_requests());

  // Remove the first predicate and expect no WatchPages message.
  ExpectNoWatchPagesMessage(tab.get(), [this, group1]() {
    tracker_.StopTrackingPredicates(std::vector<const void*>(1, group1));
  });
  EXPECT_EQ(0, delegate_.evaluation_requests());

  // Remove the second predicate and expect an empty WatchPages message.
  ExpectWatchPagesMessage(tab.get(), std::set<std::string>(),
                          [this, group2]() {
    tracker_.StopTrackingPredicates(std::vector<const void*>(1, group2));
  });
  EXPECT_EQ(0, delegate_.evaluation_requests());
}

// Tests that WatchedPageChange messages result in evaluation requests, and
// cause the appropriate predicates to evaluate to true.
TEST_F(DeclarativeContentCssConditionTrackerTest, WatchedPageChange) {
  int expected_evaluation_requests = 0;

  const std::unique_ptr<content::WebContents> tab = MakeTab();
  tracker_.TrackForWebContents(tab.get());
  EXPECT_EQ(expected_evaluation_requests, delegate_.evaluation_requests());

  std::unique_ptr<const ContentPredicate> div_predicate =
      CreatePredicate("[\"div\"]");
  std::unique_ptr<const ContentPredicate> a_predicate =
      CreatePredicate("[\"a\"]");

  std::map<const void*, std::vector<const ContentPredicate*>> predicate_groups;
  const void* const group = GeneratePredicateGroupID();
  predicate_groups[group].push_back(div_predicate.get());
  predicate_groups[group].push_back(a_predicate.get());
  tracker_.TrackPredicates(predicate_groups);
  EXPECT_EQ(expected_evaluation_requests, delegate_.evaluation_requests());

  // Check that receiving an OnWatchedPageChange message from the tab results in
  // a request for condition evaluation.
  const std::vector<std::string> matched_selectors(1, "div");
  SimulateWatchedPageChanged(tab.get(), matched_selectors);
  EXPECT_EQ(++expected_evaluation_requests, delegate_.evaluation_requests());

  // Check that only the div predicate matches.
  EXPECT_TRUE(tracker_.EvaluatePredicate(div_predicate.get(), tab.get()));
  EXPECT_FALSE(tracker_.EvaluatePredicate(a_predicate.get(), tab.get()));
  EXPECT_EQ(expected_evaluation_requests, delegate_.evaluation_requests());

  tracker_.StopTrackingPredicates(std::vector<const void*>(1, group));
}

// Tests in-page and non-in-page navigations. Only the latter should reset
// matching selectors and result in an evaluation request.
TEST_F(DeclarativeContentCssConditionTrackerTest, Navigation) {
  DeclarativeContentCssConditionTracker tracker(&delegate_);

  int expected_evaluation_requests = 0;

  const std::unique_ptr<content::WebContents> tab = MakeTab();
  tracker_.TrackForWebContents(tab.get());
  EXPECT_EQ(expected_evaluation_requests, delegate_.evaluation_requests());

  std::unique_ptr<const ContentPredicate> predicate =
      CreatePredicate("[\"div\"]");
  std::map<const void*, std::vector<const ContentPredicate*>> predicate_groups;
  const void* const group = GeneratePredicateGroupID();
  predicate_groups[group].push_back(predicate.get());
  tracker_.TrackPredicates(predicate_groups);
  EXPECT_EQ(expected_evaluation_requests, delegate_.evaluation_requests());

  // Set up the tab to have a matching selector.
  const std::vector<std::string> matched_selectors(1, "div");
  SimulateWatchedPageChanged(tab.get(), matched_selectors);
  EXPECT_EQ(++expected_evaluation_requests, delegate_.evaluation_requests());

  // Check that an in-page navigation has no effect on the matching selectors.
  {
    content::MockNavigationHandle test_handle;
    test_handle.set_has_committed(true);
    test_handle.set_is_same_document(true);
    tracker_.OnWebContentsNavigation(tab.get(), &test_handle);
    EXPECT_TRUE(tracker_.EvaluatePredicate(predicate.get(), tab.get()));
    EXPECT_EQ(expected_evaluation_requests, delegate_.evaluation_requests());
  }

  // Check that a non in-page navigation clears the matching selectors and
  // requests condition evaluation.
  {
    content::MockNavigationHandle test_handle;
    test_handle.set_has_committed(true);
    tracker_.OnWebContentsNavigation(tab.get(), &test_handle);
    EXPECT_FALSE(tracker_.EvaluatePredicate(predicate.get(), tab.get()));
    EXPECT_EQ(++expected_evaluation_requests, delegate_.evaluation_requests());
  }
}

// https://crbug.com/497586
TEST_F(DeclarativeContentCssConditionTrackerTest, WebContentsOutlivesTracker) {
  const std::unique_ptr<content::WebContents> tab = MakeTab();

  {
    DeclarativeContentCssConditionTracker tracker(&delegate_);
    tracker_.TrackForWebContents(tab.get());
  }
}

}  // namespace extensions
