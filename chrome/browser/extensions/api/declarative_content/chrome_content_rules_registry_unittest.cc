// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_content/chrome_content_rules_registry.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/extensions/api/declarative_content/content_predicate.h"
#include "chrome/browser/extensions/api/declarative_content/content_predicate_evaluator.h"
#include "chrome/browser/extensions/test_extension_environment.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_renderer_host.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

class TestPredicateEvaluator;

class TestPredicate : public ContentPredicate {
 public:
  explicit TestPredicate(ContentPredicateEvaluator* evaluator)
      : evaluator_(evaluator) {
  }

  TestPredicate(const TestPredicate&) = delete;
  TestPredicate& operator=(const TestPredicate&) = delete;

  ContentPredicateEvaluator* GetEvaluator() const override {
    return evaluator_;
  }

 private:
  raw_ptr<ContentPredicateEvaluator, DanglingUntriaged> evaluator_;
};

class TestPredicateEvaluator : public ContentPredicateEvaluator {
 public:
  explicit TestPredicateEvaluator(ContentPredicateEvaluator::Delegate* delegate)
      : delegate_(delegate),
        contents_for_next_operation_evaluation_(nullptr),
        next_evaluation_result_(false) {
  }

  TestPredicateEvaluator(const TestPredicateEvaluator&) = delete;
  TestPredicateEvaluator& operator=(const TestPredicateEvaluator&) = delete;

  std::string GetPredicateApiAttributeName() const override {
    return "test_predicate";
  }

  std::unique_ptr<const ContentPredicate> CreatePredicate(
      const Extension* extension,
      const base::Value& value,
      std::string* error) override {
    RequestEvaluationIfSpecified();
    return std::make_unique<TestPredicate>(this);
  }

  void TrackPredicates(
      const std::map<const void*,
          std::vector<const ContentPredicate*>>& predicates) override {
    RequestEvaluationIfSpecified();
  }

  void StopTrackingPredicates(
      const std::vector<const void*>& predicate_groups) override {
    RequestEvaluationIfSpecified();
  }

  void TrackForWebContents(content::WebContents* contents) override {
    RequestEvaluationIfSpecified();
  }

  void OnWebContentsNavigation(
      content::WebContents* contents,
      content::NavigationHandle* navigation_handle) override {
    RequestEvaluationIfSpecified();
  }

  void OnWatchedPageChanged(
      content::WebContents* contents,
      const std::vector<std::string>& css_selectors) override {
    RequestEvaluationIfSpecified();
  }

  bool EvaluatePredicate(const ContentPredicate* predicate,
                         content::WebContents* tab) const override {
    bool result = next_evaluation_result_;
    next_evaluation_result_ = false;
    return result;
  }

  void RequestImmediateEvaluation(content::WebContents* contents,
                                  bool evaluation_result) {
    next_evaluation_result_ = evaluation_result;
    delegate_->RequestEvaluation(contents);
  }

  void RequestEvaluationOnNextOperation(content::WebContents* contents,
                                        bool evaluation_result) {
    contents_for_next_operation_evaluation_ = contents;
    next_evaluation_result_ = evaluation_result;
  }

 private:
  void RequestEvaluationIfSpecified() {
    if (contents_for_next_operation_evaluation_) {
      delegate_->RequestEvaluation(contents_for_next_operation_evaluation_);
    }
    contents_for_next_operation_evaluation_ = nullptr;
  }

  raw_ptr<ContentPredicateEvaluator::Delegate> delegate_;
  raw_ptr<content::WebContents> contents_for_next_operation_evaluation_;
  mutable bool next_evaluation_result_;
};

// Create the test evaluator and set |evaluator| to its pointer.
std::vector<std::unique_ptr<ContentPredicateEvaluator>> CreateTestEvaluator(
    TestPredicateEvaluator** evaluator,
    ContentPredicateEvaluator::Delegate* delegate) {
  std::vector<std::unique_ptr<ContentPredicateEvaluator>> evaluators;
  *evaluator = new TestPredicateEvaluator(delegate);
  evaluators.push_back(std::unique_ptr<ContentPredicateEvaluator>(*evaluator));
  return evaluators;
}

}  // namespace

class DeclarativeChromeContentRulesRegistryTest : public testing::Test {
 public:
  DeclarativeChromeContentRulesRegistryTest() {}

  DeclarativeChromeContentRulesRegistryTest(
      const DeclarativeChromeContentRulesRegistryTest&) = delete;
  DeclarativeChromeContentRulesRegistryTest& operator=(
      const DeclarativeChromeContentRulesRegistryTest&) = delete;

 protected:
  TestExtensionEnvironment* env() { return &env_; }

 private:
  TestExtensionEnvironment env_;

  // Must come after |env_| so only one UI MessageLoop is created.
  content::RenderViewHostTestEnabler rvh_enabler_;
};

TEST_F(DeclarativeChromeContentRulesRegistryTest, ActiveRulesDoesntGrow) {
  TestPredicateEvaluator* evaluator = nullptr;
  scoped_refptr<ChromeContentRulesRegistry> registry(
      new ChromeContentRulesRegistry(
          env()->profile(), nullptr,
          base::BindOnce(&CreateTestEvaluator, &evaluator)));

  EXPECT_EQ(0u, registry->GetActiveRulesCountForTesting());

  std::unique_ptr<content::WebContents> tab = env()->MakeTab();
  registry->MonitorWebContentsForRuleEvaluation(tab.get());
  content::MockNavigationHandle navigation_handle;
  navigation_handle.set_has_committed(true);

  registry->DidFinishNavigation(tab.get(), &navigation_handle);
  EXPECT_EQ(0u, registry->GetActiveRulesCountForTesting());

  // Add a rule.
  auto rule = api::events::Rule::FromValue(base::test::ParseJsonDict(R"({
          "id": "rule1",
          "priority": 100,
          "conditions": [
           {
             "instanceType": "declarativeContent.PageStateMatcher",
             "test_predicate": []
           }],
          "actions": [
            {"instanceType": "declarativeContent.ShowAction"}
          ]
      })"));
  ASSERT_TRUE(rule.has_value());
  std::vector<const api::events::Rule*> rules({&rule.value()});

  const Extension* extension =
      env()->MakeExtension(base::test::ParseJsonDict("{\"page_action\": {}}"));
  registry->AddRulesImpl(extension->id(), rules);

  registry->DidFinishNavigation(tab.get(), &navigation_handle);
  EXPECT_EQ(0u, registry->GetActiveRulesCountForTesting());

  evaluator->RequestImmediateEvaluation(tab.get(), true);
  EXPECT_EQ(1u, registry->GetActiveRulesCountForTesting());

  // Closing the tab should erase its entry from active_rules_. Invoke
  // WebContentsDestroyed on the registry to mock it being notified that the tab
  // has closed.
  registry->WebContentsDestroyed(tab.get());
  tab.reset();
  EXPECT_EQ(0u, registry->GetActiveRulesCountForTesting());

  tab = env()->MakeTab();
  content::MockNavigationHandle navigation_handle2;
  navigation_handle2.set_has_committed(true);
  registry->MonitorWebContentsForRuleEvaluation(tab.get());
  evaluator->RequestImmediateEvaluation(tab.get(), true);
  EXPECT_EQ(1u, registry->GetActiveRulesCountForTesting());

  evaluator->RequestEvaluationOnNextOperation(tab.get(), false);
  registry->DidFinishNavigation(tab.get(), &navigation_handle2);
  EXPECT_EQ(0u, registry->GetActiveRulesCountForTesting());
}

}  // namespace extensions
