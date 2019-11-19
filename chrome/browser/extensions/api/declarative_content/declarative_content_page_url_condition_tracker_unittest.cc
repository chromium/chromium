// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_content/declarative_content_page_url_condition_tracker.h"

#include <set>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/stl_util.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/extensions/api/declarative_content/content_predicate_evaluator.h"
#include "chrome/browser/extensions/api/declarative_content/declarative_content_condition_tracker_test.h"
#include "components/url_matcher/url_matcher.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions {

using testing::ElementsAre;
using testing::HasSubstr;
using testing::UnorderedElementsAre;
using testing::UnorderedElementsAreArray;

class DeclarativeContentPageUrlConditionTrackerTest
    : public DeclarativeContentConditionTrackerTest {
 protected:
  class Delegate : public ContentPredicateEvaluator::Delegate {
   public:
    Delegate() {}

    std::set<content::WebContents*>& evaluation_requests() {
      return evaluation_requests_;
    }

    // ContentPredicateEvaluator::Delegate:
    void RequestEvaluation(content::WebContents* contents) override {
      EXPECT_FALSE(base::Contains(evaluation_requests_, contents));
      evaluation_requests_.insert(contents);
    }

    bool ShouldManageConditionsForBrowserContext(
        content::BrowserContext* context) override {
      return true;
    }

   private:
    std::set<content::WebContents*> evaluation_requests_;

    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  DeclarativeContentPageUrlConditionTrackerTest()
      : tracker_(&delegate_) {
  }

  // Creates a predicate with appropriate expectations of success.
  std::unique_ptr<const ContentPredicate> CreatePredicate(
      const std::string& value) {
    std::unique_ptr<const ContentPredicate> predicate;
    CreatePredicateImpl(value, &predicate);
    return predicate;
  }

  void LoadURL(content::WebContents* tab, const GURL& url) {
    tab->GetController().LoadURL(url, content::Referrer(),
                                 ui::PAGE_TRANSITION_LINK, std::string());
  }

  Delegate delegate_;
  DeclarativeContentPageUrlConditionTracker tracker_;

 private:
  // This function exists to work around the gtest limitation that functions
  // with fatal assertions must return void.
  void CreatePredicateImpl(const std::string& value,
                           std::unique_ptr<const ContentPredicate>* predicate) {
    std::string error;
    *predicate = tracker_.CreatePredicate(
        nullptr, *base::test::ParseJsonDeprecated(value), &error);
    EXPECT_EQ("", error);
    ASSERT_TRUE(*predicate);
  }

  DISALLOW_COPY_AND_ASSIGN(DeclarativeContentPageUrlConditionTrackerTest);
};

TEST(DeclarativeContentPageUrlPredicateTest, WrongPageUrlDatatype) {
  url_matcher::URLMatcher matcher;
  std::string error;
  std::unique_ptr<DeclarativeContentPageUrlPredicate> predicate =
      DeclarativeContentPageUrlPredicate::Create(
          nullptr, matcher.condition_factory(),
          *base::test::ParseJsonDeprecated("[]"), &error);
  EXPECT_THAT(error, HasSubstr("invalid type"));
  EXPECT_FALSE(predicate);

  EXPECT_TRUE(matcher.IsEmpty()) << "Errors shouldn't add URL conditions";
}

TEST(DeclarativeContentPageUrlPredicateTest, PageUrlPredicate) {
  url_matcher::URLMatcher matcher;
  std::string error;
  std::unique_ptr<DeclarativeContentPageUrlPredicate> predicate =
      DeclarativeContentPageUrlPredicate::Create(
          nullptr, matcher.condition_factory(),
          *base::test::ParseJsonDeprecated("{\"hostSuffix\": \"example.com\"}"),
          &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(predicate);

  url_matcher::URLMatcherConditionSet::Vector all_new_condition_sets;
  all_new_condition_sets.push_back(predicate->url_matcher_condition_set());
  matcher.AddConditionSets(all_new_condition_sets);
  EXPECT_FALSE(matcher.IsEmpty());

  EXPECT_THAT(matcher.MatchURL(GURL("http://google.com/")),
              ElementsAre(/*empty*/));
  std::set<url_matcher::URLMatcherConditionSet::ID> page_url_matches =
      matcher.MatchURL(GURL("http://www.example.com/foobar"));
  EXPECT_THAT(
      page_url_matches,
      ElementsAre(predicate->url_matcher_condition_set()->id()));
}

// Tests that adding and removing condition sets trigger evaluation requests for
// the matching WebContents.
TEST_F(DeclarativeContentPageUrlConditionTrackerTest, AddAndRemovePredicates) {
  // Create four tabs.
  std::vector<std::unique_ptr<content::WebContents>> tabs;
  for (int i = 0; i < 4; ++i) {
    tabs.push_back(MakeTab());
    delegate_.evaluation_requests().clear();
    tracker_.TrackForWebContents(tabs.back().get());
    EXPECT_THAT(delegate_.evaluation_requests(),
                UnorderedElementsAre(tabs.back().get()));
  }

  // Navigate three of them to URLs that will match with predicats we're about
  // to add.
  LoadURL(tabs[0].get(), GURL("http://test1/"));
  LoadURL(tabs[1].get(), GURL("http://test2/"));
  LoadURL(tabs[2].get(), GURL("http://test3/"));

  std::vector<std::unique_ptr<const ContentPredicate>> predicates;
  std::string error;
  predicates.push_back(CreatePredicate("{\"hostPrefix\": \"test1\"}"));
  predicates.push_back(CreatePredicate("{\"hostPrefix\": \"test2\"}"));
  predicates.push_back(CreatePredicate("{\"hostPrefix\": \"test3\"}"));

  // Add the predicates in two groups: (0, 1) and (2).
  delegate_.evaluation_requests().clear();
  std::map<const void*, std::vector<const ContentPredicate*>> predicate_groups;
  const void* const group1 = GeneratePredicateGroupID();
  predicate_groups[group1].push_back(predicates[0].get());
  predicate_groups[group1].push_back(predicates[1].get());
  const void* const group2 = GeneratePredicateGroupID();
  predicate_groups[group2].push_back(predicates[2].get());
  tracker_.TrackPredicates(predicate_groups);
  EXPECT_THAT(
      delegate_.evaluation_requests(),
      UnorderedElementsAre(tabs[0].get(), tabs[1].get(), tabs[2].get()));

  // Check that the predicates evaluate as expected for the tabs.
  EXPECT_TRUE(tracker_.EvaluatePredicate(predicates[0].get(), tabs[0].get()));
  EXPECT_FALSE(tracker_.EvaluatePredicate(predicates[0].get(), tabs[1].get()));
  EXPECT_FALSE(tracker_.EvaluatePredicate(predicates[0].get(), tabs[2].get()));
  EXPECT_FALSE(tracker_.EvaluatePredicate(predicates[0].get(), tabs[3].get()));

  EXPECT_FALSE(tracker_.EvaluatePredicate(predicates[1].get(), tabs[0].get()));
  EXPECT_TRUE(tracker_.EvaluatePredicate(predicates[1].get(), tabs[1].get()));
  EXPECT_FALSE(tracker_.EvaluatePredicate(predicates[1].get(), tabs[2].get()));
  EXPECT_FALSE(tracker_.EvaluatePredicate(predicates[1].get(), tabs[3].get()));

  EXPECT_FALSE(tracker_.EvaluatePredicate(predicates[2].get(), tabs[0].get()));
  EXPECT_FALSE(tracker_.EvaluatePredicate(predicates[2].get(), tabs[1].get()));
  EXPECT_TRUE(tracker_.EvaluatePredicate(predicates[2].get(), tabs[2].get()));
  EXPECT_FALSE(tracker_.EvaluatePredicate(predicates[2].get(), tabs[3].get()));

  // Remove the first group of predicates.
  delegate_.evaluation_requests().clear();
  tracker_.StopTrackingPredicates(std::vector<const void*>(1, group1));
  EXPECT_THAT(delegate_.evaluation_requests(),
              UnorderedElementsAre(tabs[0].get(), tabs[1].get()));

  // Remove the second group of predicates.
  delegate_.evaluation_requests().clear();
  tracker_.StopTrackingPredicates(std::vector<const void*>(1, group2));
  EXPECT_THAT(delegate_.evaluation_requests(),
              UnorderedElementsAre(tabs[2].get()));
}

// Tests that tracking WebContents triggers evaluation requests for matching
// rules.
TEST_F(DeclarativeContentPageUrlConditionTrackerTest, TrackWebContents) {
  std::string error;
  std::unique_ptr<const ContentPredicate> predicate =
      CreatePredicate("{\"hostPrefix\": \"test1\"}");

  delegate_.evaluation_requests().clear();
  std::map<const void*, std::vector<const ContentPredicate*>> predicates;
  const void* const group = GeneratePredicateGroupID();
  predicates[group].push_back(predicate.get());
  tracker_.TrackPredicates(predicates);
  EXPECT_TRUE(delegate_.evaluation_requests().empty());

  const std::unique_ptr<content::WebContents> matching_tab = MakeTab();
  LoadURL(matching_tab.get(), GURL("http://test1/"));

  tracker_.TrackForWebContents(matching_tab.get());
  EXPECT_THAT(delegate_.evaluation_requests(),
              UnorderedElementsAre(matching_tab.get()));

  delegate_.evaluation_requests().clear();
  const std::unique_ptr<content::WebContents> non_matching_tab = MakeTab();
  tracker_.TrackForWebContents(non_matching_tab.get());
  EXPECT_THAT(delegate_.evaluation_requests(),
              UnorderedElementsAre(non_matching_tab.get()));

  delegate_.evaluation_requests().clear();
  tracker_.StopTrackingPredicates(std::vector<const void*>(1, group));
  EXPECT_THAT(delegate_.evaluation_requests(),
              UnorderedElementsAre(matching_tab.get()));
}

// Tests that notifying WebContents navigation triggers evaluation requests for
// matching rules.
TEST_F(DeclarativeContentPageUrlConditionTrackerTest,
       NotifyWebContentsNavigation) {
  std::string error;
  std::unique_ptr<const ContentPredicate> predicate =
      CreatePredicate("{\"hostPrefix\": \"test1\"}");

  delegate_.evaluation_requests().clear();
  std::map<const void*, std::vector<const ContentPredicate*>> predicates;
  const void* const group = GeneratePredicateGroupID();
  predicates[group].push_back(predicate.get());
  tracker_.TrackPredicates(predicates);
  EXPECT_TRUE(delegate_.evaluation_requests().empty());

  const std::unique_ptr<content::WebContents> tab = MakeTab();
  tracker_.TrackForWebContents(tab.get());
  EXPECT_THAT(delegate_.evaluation_requests(),
              UnorderedElementsAre(tab.get()));

  // Check that navigation notification to a matching URL results in an
  // evaluation request.
  LoadURL(tab.get(), GURL("http://test1/"));
  delegate_.evaluation_requests().clear();
  tracker_.OnWebContentsNavigation(tab.get(), nullptr);
  EXPECT_THAT(delegate_.evaluation_requests(),
              UnorderedElementsAre(tab.get()));

  // Check that navigation notification from a matching URL to another matching
  // URL results in an evaluation request.
  LoadURL(tab.get(), GURL("http://test1/a"));
  delegate_.evaluation_requests().clear();
  tracker_.OnWebContentsNavigation(tab.get(), nullptr);
  EXPECT_THAT(delegate_.evaluation_requests(),
              UnorderedElementsAre(tab.get()));

  // Check that navigation notification from a matching URL to a non-matching
  // URL results in an evaluation request.
  delegate_.evaluation_requests().clear();
  LoadURL(tab.get(), GURL("http://test2/"));
  tracker_.OnWebContentsNavigation(tab.get(), nullptr);
  EXPECT_THAT(delegate_.evaluation_requests(),
              UnorderedElementsAre(tab.get()));

  // Check that navigation notification from a non-matching URL to another
  // non-matching URL results in an evaluation request.
  delegate_.evaluation_requests().clear();
  LoadURL(tab.get(), GURL("http://test2/a"));
  tracker_.OnWebContentsNavigation(tab.get(), nullptr);
  EXPECT_THAT(delegate_.evaluation_requests(),
              UnorderedElementsAre(tab.get()));

  delegate_.evaluation_requests().clear();
  tracker_.StopTrackingPredicates(std::vector<const void*>(1, group));
  EXPECT_THAT(delegate_.evaluation_requests(),
              UnorderedElementsAre(/* empty */));
}

}  // namespace extensions
