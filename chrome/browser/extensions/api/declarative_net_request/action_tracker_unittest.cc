// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "extensions/browser/api/declarative_net_request/action_tracker.h"

#include <string_view>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/api/declarative_net_request/dnr_test_base.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/api/declarative_net_request/test_utils.h"
#include "extensions/common/constants.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"
#include "url/gurl.h"

namespace extensions {
namespace declarative_net_request {

namespace {

constexpr int64_t kNavigationId = 1;

class ActionTrackerTest : public DNRTestBase {
 public:
  ActionTrackerTest() = default;
  ActionTrackerTest(const ActionTrackerTest& other) = delete;
  ActionTrackerTest& operator=(const ActionTrackerTest& other) = delete;

  void SetUp() override {
    DNRTestBase::SetUp();
    action_tracker_ = std::make_unique<ActionTracker>(browser_context());

    // Do not check whether tab IDs correspond to valid tabs in this test as
    // this is a unit test and no actual tabs will be created.
    action_tracker_->SetCheckTabIdOnRuleMatchForTest(false);
  }

 protected:
  using RequestActionType = RequestAction::Type;

  // Helper to load an extension. |flags| is a bitmask of ConfigFlag to
  // configure the extension.
  void LoadExtension(const std::string& extension_dirname, unsigned flags) {
    base::FilePath extension_dir =
        temp_dir().GetPath().AppendASCII(extension_dirname);

    // Create extension directory.
    ASSERT_TRUE(base::CreateDirectory(extension_dir));
    constexpr char kRulesetID[] = "id";
    constexpr char kJSONRulesFilename[] = "rules_file.json";
    TestRulesetInfo info(kRulesetID, kJSONRulesFilename, base::Value::List());
    WriteManifestAndRuleset(
        extension_dir, info,
        std::vector<std::string>({URLPattern::kAllUrlsPattern}), flags);

    last_loaded_extension_ =
        CreateExtensionLoader()->LoadExtension(extension_dir);
    ASSERT_TRUE(last_loaded_extension_);

    ExtensionRegistry::Get(browser_context())
        ->AddEnabled(last_loaded_extension_);
  }

  // Helper to create a RequestAction for the given |extension_id|.
  RequestAction CreateRequestAction(const ExtensionId& extension_id) {
    return RequestAction(RequestActionType::BLOCK, kMinValidID,
                         kDefaultPriority, kMinValidStaticRulesetID,
                         extension_id);
  }

  // Returns renderer-initiated request params for the given |url|.
  WebRequestInfoInitParams GetRequestParamsForURL(
      std::string_view url,
      WebRequestResourceType web_request_type,
      int tab_id) {
    const int kRendererId = 1;
    WebRequestInfoInitParams info;
    info.url = GURL(url);
    info.web_request_type = web_request_type;
    info.render_process_id = kRendererId;
    info.frame_data.tab_id = tab_id;

    if (web_request_type == WebRequestResourceType::MAIN_FRAME) {
      info.navigation_id = kNavigationId;
      info.is_navigation_request = true;
    }

    return info;
  }

  const Extension* last_loaded_extension() const {
    return last_loaded_extension_.get();
  }

  ActionTracker* action_tracker() { return action_tracker_.get(); }

 private:
  scoped_refptr<const Extension> last_loaded_extension_;
  std::unique_ptr<ActionTracker> action_tracker_;
};

// Test that rules matched will be recorded for extensions with the
// declarativeNetRequestFeedback or activeTab permission.
TEST_P(ActionTrackerTest, GetMatchedRulesNoPermission) {
  // Load an extension with the declarativeNetRequestFeedback permission.
  ASSERT_NO_FATAL_FAILURE(LoadExtension(
      "test_extension", ConfigFlag::kConfig_HasFeedbackPermission));
  const Extension* extension_1 = last_loaded_extension();

  // Load an extension without the declarativeNetRequestFeedback permission.
  ASSERT_NO_FATAL_FAILURE(
      LoadExtension("test_extension_2", ConfigFlag::kConfig_None));
  const Extension* extension_2 = last_loaded_extension();

  // Load an extension without the declarativeNetRequestFeedback permission but
  // with the activeTab permission.
  ASSERT_NO_FATAL_FAILURE(
      LoadExtension("test_extension_3", ConfigFlag::kConfig_HasActiveTab));
  const Extension* extension_3 = last_loaded_extension();

  const int tab_id = 1;

  // Record a rule match for a main-frame navigation request.
  WebRequestInfo request_1(GetRequestParamsForURL(
      "http://one.com", WebRequestResourceType::MAIN_FRAME, tab_id));

  // Record a rule match for a non-navigation request.
  WebRequestInfo request_2(GetRequestParamsForURL(
      "http://one.com", WebRequestResourceType::OTHER, tab_id));

  // Assume a rule is matched for |request_1| and |request_2| for all three
  // extensions.
  for (const Extension* extension : {extension_1, extension_2, extension_3}) {
    action_tracker()->OnRuleMatched(CreateRequestAction(extension->id()),
                                    request_1);
    action_tracker()->OnRuleMatched(CreateRequestAction(extension->id()),
                                    request_2);
  }

  // No need to trim non-active rules because all requests are from active tabs.
  const bool trim_non_active_rules = false;

  // For |extension_1|, one rule match should be recorded for |rules_tracked_|
  // and one for |pending_navigation_actions_|.
  EXPECT_EQ(1, action_tracker()->GetMatchedRuleCountForTest(
                   extension_1->id(), tab_id, trim_non_active_rules));
  EXPECT_EQ(1, action_tracker()->GetPendingRuleCountForTest(extension_1->id(),
                                                            kNavigationId));

  // Since |extension_2| does not have the feedback permission, no rule matches
  // should be recorded.
  EXPECT_EQ(0, action_tracker()->GetMatchedRuleCountForTest(
                   extension_2->id(), tab_id, trim_non_active_rules));
  EXPECT_EQ(0, action_tracker()->GetPendingRuleCountForTest(extension_2->id(),
                                                            kNavigationId));

  // While |extension_3| does not have the feedback permission, it does have the
  // activeTab permission and therefore rule matches should be recorded.
  EXPECT_EQ(1, action_tracker()->GetMatchedRuleCountForTest(
                   extension_3->id(), tab_id, trim_non_active_rules));
  EXPECT_EQ(1, action_tracker()->GetPendingRuleCountForTest(extension_3->id(),
                                                            kNavigationId));

  // Clean up the internal state of |action_tracker_|.
  action_tracker()->ClearPendingNavigation(kNavigationId);
  action_tracker()->ClearTabData(tab_id);
}

// Test that rules associated with a non-active tab past their lifetime will be
// removed, and not returned by getMatchedRules
TEST_P(ActionTrackerTest, GetMatchedRulesLifespan) {
  base::SimpleTestClock clock_;
  clock_.SetNow(base::Time::Now());
  action_tracker()->SetClockForTests(&clock_);

  // Load an extension with the declarativeNetRequestFeedback permission.
  ASSERT_NO_FATAL_FAILURE(LoadExtension(
      "test_extension", ConfigFlag::kConfig_HasFeedbackPermission));
  const Extension* extension_1 = last_loaded_extension();

  const int tab_id = 1;

  // Record a rule match for a non-navigation request.
  WebRequestInfo request_1(GetRequestParamsForURL(
      "http://one.com", WebRequestResourceType::OTHER, tab_id));
  action_tracker()->OnRuleMatched(CreateRequestAction(extension_1->id()),
                                  request_1);

  // Half life of a matched rule associated with a non-active tab, with 50ms
  // added.
  base::TimeDelta half_life =
      (ActionTracker::kNonActiveTabRuleLifespan / 2) + base::Milliseconds(50);

  // Advance the clock by half of the lifespan of a matched rule for the unknown
  // tab ID.
  clock_.Advance(half_life);

  // Record another rule match.
  action_tracker()->OnRuleMatched(CreateRequestAction(extension_1->id()),
                                  request_1);

  // Close the tab with |tab_id|.
  action_tracker()->ClearTabData(tab_id);

  // Since some requests are not from an active tab, and the rule count for the
  // unknown tab will be queried, we should emulate the getMatchedRules API call
  // and trim non-active rules.
  const bool trim_non_active_rules = true;

  // Both rules should now be attributed to the unknown tab ID since the tab in
  // which they were matched is no longer active.
  EXPECT_EQ(2, action_tracker()->GetMatchedRuleCountForTest(
                   extension_1->id(), extension_misc::kUnknownTabId,
                   trim_non_active_rules));

  // Advance the clock so one of the matched rules will be older than the
  // lifespan for rules not associated with an active tab.
  clock_.Advance(half_life);

  // Rules not attributed to an active tab have a fixed lifespan,
  // so only one rule should be returned.
  EXPECT_EQ(1, action_tracker()->GetMatchedRuleCountForTest(
                   extension_1->id(), extension_misc::kUnknownTabId,
                   trim_non_active_rules));

  // Advance the clock so both rules will be older than the matched rule
  // lifespan.
  clock_.Advance(ActionTracker::kNonActiveTabRuleLifespan);

  // Since both rules are older than the lifespan, they should be cleared and no
  // rules are returned.
  EXPECT_EQ(0, action_tracker()->GetMatchedRuleCountForTest(
                   extension_1->id(), extension_misc::kUnknownTabId,
                   trim_non_active_rules));

  action_tracker()->SetClockForTests(nullptr);
}

// Test that matched rules not associated with an active tab will be
// automatically cleaned up by a recurring task.
TEST_P(ActionTrackerTest, RulesClearedOnTimer) {
  scoped_refptr<base::TestMockTimeTaskRunner> mock_time_task_runner =
      base::MakeRefCounted<base::TestMockTimeTaskRunner>(
          base::Time::Now(), base::TimeTicks::Now());
  // Mock the clock and the timer. Clock is used to check whether the rule
  // should be removed and the timer is used to set up a recurrent task to clean
  // up old rules.
  action_tracker()->SetClockForTests(mock_time_task_runner->GetMockClock());

  auto mock_trim_timer = std::make_unique<base::RetainingOneShotTimer>(
      mock_time_task_runner->GetMockTickClock());

  mock_trim_timer->SetTaskRunner(mock_time_task_runner);
  action_tracker()->SetTimerForTest(std::move(mock_trim_timer));

  // Load an extension with the declarativeNetRequestFeedback permission.
  ASSERT_NO_FATAL_FAILURE(LoadExtension(
      "test_extension", ConfigFlag::kConfig_HasFeedbackPermission));
  const Extension* extension_1 = last_loaded_extension();

  // Record a rule match for |extension_1| for the unknown tab.
  WebRequestInfo request_1(
      GetRequestParamsForURL("http://one.com", WebRequestResourceType::OTHER,
                             extension_misc::kUnknownTabId));
  action_tracker()->OnRuleMatched(CreateRequestAction(extension_1->id()),
                                  request_1);

  // Since this test explicitly tests that the recurring task will remove
  // non-active rules, ensure that calls to GetmatchedRuleCountForTest will not
  // remove non-active rules.
  const bool trim_non_active_rules = false;

  // Verify that the rule has been matched.
  EXPECT_EQ(1, action_tracker()->GetMatchedRuleCountForTest(
                   extension_1->id(), extension_misc::kUnknownTabId,
                   trim_non_active_rules));

  // Advance the clock by more than the lifespan of a rule through
  // |mock_time_task_runner|.
  mock_time_task_runner->FastForwardBy(
      ActionTracker::kNonActiveTabRuleLifespan + base::Seconds(1));

  // Verify that the rule has been cleared by the recurring task.
  EXPECT_EQ(0, action_tracker()->GetMatchedRuleCountForTest(
                   extension_1->id(), extension_misc::kUnknownTabId,
                   trim_non_active_rules));

  // Reset the ActionTracker's state.
  action_tracker()->SetClockForTests(nullptr);
  action_tracker()->SetTimerForTest(
      std::make_unique<base::RetainingOneShotTimer>());
}

INSTANTIATE_TEST_SUITE_P(All,
                         ActionTrackerTest,
                         ::testing::Values(ExtensionLoadType::PACKED,
                                           ExtensionLoadType::UNPACKED));

}  // namespace
}  // namespace declarative_net_request
}  // namespace extensions
