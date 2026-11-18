// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/mojom/window_open_disposition.mojom.h"

namespace {

constexpr char kUnloaded[] = "unloaded";

// Tests the "unloaded" subtype of tab targets when Chrome discards a tab with
// TabLifecycleUnit. The parameter is the state of the WebContentsDiscard
// feature. With the feature, a discard keeps the WebContents of the tab
// (in-place discard). Without the feature (the default on desktop), a discard
// replaces the WebContents.
class DevToolsTabDiscardTest : public DevToolsProtocolTestBase,
                               public testing::WithParamInterface<bool> {
 public:
  DevToolsTabDiscardTest() {
    feature_list_.InitWithFeatureState(features::kWebContentsDiscard,
                                       IsInPlaceDiscard());
  }

  static bool IsInPlaceDiscard() { return GetParam(); }

  void SetUpOnMainThread() override {
    DevToolsProtocolTestBase::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  static base::ListValue TabFilter() {
    base::DictValue filter_entry;
    filter_entry.Set("type", "tab");
    base::ListValue filter;
    filter.Append(std::move(filter_entry));
    return filter;
  }

  // Returns the subtype from Target.getTargetInfo, or an empty string if the
  // target info has no subtype.
  std::string GetSubtype(const std::string& target_id) {
    base::DictValue params;
    params.Set("targetId", target_id);
    const base::DictValue* result =
        SendCommandSync("Target.getTargetInfo", std::move(params));
    if (!result) {
      ADD_FAILURE() << "Target.getTargetInfo failed for " << target_id;
      return std::string();
    }
    const std::string* subtype =
        result->FindStringByDottedPath("targetInfo.subtype");
    return subtype ? *subtype : std::string();
  }

  // Waits for Target.targetInfoChanged for `target_id` with `subtype`. An empty
  // `subtype` matches target info without a subtype.
  void WaitForSubtypeChanged(const std::string& target_id,
                             const std::string& subtype) {
    WaitForMatchingNotification(
        "Target.targetInfoChanged",
        base::BindRepeating(
            [](const std::string& target_id, const std::string& subtype,
               const base::DictValue& params) {
              const std::string* id =
                  params.FindStringByDottedPath("targetInfo.targetId");
              const std::string* actual =
                  params.FindStringByDottedPath("targetInfo.subtype");
              return id && *id == target_id &&
                     (actual ? *actual : std::string()) == subtype;
            },
            target_id, subtype));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(DevToolsTabDiscardTest, DiscardReportsUnloaded) {
  TabStripModel* tab_strip_model = browser()->GetTabStripModel();
  const GURL url = embedded_test_server()->GetURL("/title1.html");
  // TabLifecycleUnit does not discard the active tab, so discard a tab in the
  // background.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::kNewBackgroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ASSERT_EQ(2, tab_strip_model->count());
  content::WebContents* old_contents = tab_strip_model->GetWebContentsAt(1);

  // Discover tab targets. This forces the creation of tab hosts for new
  // WebContents. Do not attach to the tab target: an attach loads a tab
  // without a live main frame.
  AttachToBrowserTarget();
  base::DictValue discover_params;
  discover_params.Set("discover", true);
  discover_params.Set("filter", TabFilter());
  ASSERT_TRUE(
      SendCommandSync("Target.setDiscoverTargets", std::move(discover_params)));

  const std::string target_id =
      content::DevToolsAgentHost::GetOrCreateForTab(old_contents)->GetId();
  EXPECT_EQ("", GetSubtype(target_id));
  ClearNotifications();

  ASSERT_TRUE(
      resource_coordinator::TabLifecycleUnitSource::GetTabLifecycleUnitExternal(
          old_contents)
          ->DiscardTab(mojom::LifecycleUnitDiscardReason::URGENT));
  content::WebContents* new_contents = tab_strip_model->GetWebContentsAt(1);
  if (IsInPlaceDiscard()) {
    ASSERT_EQ(old_contents, new_contents);
  } else {
    ASSERT_NE(old_contents, new_contents);
  }
  ASSERT_TRUE(new_contents->WasDiscarded());

  const std::string new_target_id =
      content::DevToolsAgentHost::GetOrCreateForTab(new_contents)->GetId();
  if (IsInPlaceDiscard()) {
    // The tab target keeps its ID, and reports the subtype.
    EXPECT_EQ(target_id, new_target_id);
    WaitForSubtypeChanged(target_id, kUnloaded);
  } else {
    // The tab target of the old WebContents is destroyed. The new WebContents
    // gets a new tab target, which is unloaded when it is created.
    EXPECT_NE(target_id, new_target_id);
    WaitForMatchingNotification(
        "Target.targetDestroyed",
        base::BindRepeating(
            [](const std::string& target_id, const base::DictValue& params) {
              const std::string* id = params.FindString("targetId");
              return id && *id == target_id;
            },
            target_id));
    WaitForMatchingNotification(
        "Target.targetCreated",
        base::BindRepeating(
            [](const std::string& target_id, const base::DictValue& params) {
              const std::string* id =
                  params.FindStringByDottedPath("targetInfo.targetId");
              const std::string* subtype =
                  params.FindStringByDottedPath("targetInfo.subtype");
              return id && *id == target_id && subtype && *subtype == kUnloaded;
            },
            new_target_id));
  }
  // Target.getTargetInfo makes sure that the client received all events.
  EXPECT_EQ(kUnloaded, GetSubtype(new_target_id));

  // A reload clears the subtype.
  ClearNotifications();
  content::TestNavigationObserver observer(new_contents, 1);
  new_contents->GetController().Reload(content::ReloadType::NORMAL,
                                       /*check_for_repost=*/false);
  observer.Wait();
  WaitForSubtypeChanged(new_target_id, "");
  EXPECT_EQ("", GetSubtype(new_target_id));
}

INSTANTIATE_TEST_SUITE_P(All,
                         DevToolsTabDiscardTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "InPlace" : "Replacement";
                         });

}  // namespace
