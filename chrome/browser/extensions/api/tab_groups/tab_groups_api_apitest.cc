// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/contains.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/api/tab_groups.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/test_event_router_observer.h"

namespace extensions {

namespace {

using TabGroupsApiTest = ExtensionApiTest;

// TODO(crbug.com/40910190): Test is flaky.
IN_PROC_BROWSER_TEST_F(TabGroupsApiTest, TestTabGroupsWorks) {
  ASSERT_TRUE(RunExtensionTest("tab_groups")) << message_;
}

// Tests that events are restricted to their respective browser contexts,
// especially between on-the-record and off-the-record browsers.
// Note: unit tests don't support multiple profiles, so this has to be a browser
// test.
IN_PROC_BROWSER_TEST_F(TabGroupsApiTest, TestTabGroupEventsAcrossProfiles) {
  Browser* incognito_browser =
      OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));

  // The EventRouter is shared between on- and off-the-record profiles, so
  // this observer will catch events for each. To verify that the events are
  // restricted to their respective contexts, we check the event metadata.
  TestEventRouterObserver event_observer(
      EventRouter::Get(browser()->profile()));

  browser()->tab_strip_model()->AddToNewGroup({0});
  ASSERT_TRUE(base::Contains(event_observer.events(),
                             api::tab_groups::OnCreated::kEventName));
  Event* normal_event =
      event_observer.events().at(api::tab_groups::OnCreated::kEventName).get();
  EXPECT_EQ(normal_event->restrict_to_browser_context, browser()->profile());

  event_observer.ClearEvents();

  incognito_browser->tab_strip_model()->AddToNewGroup({0});
  ASSERT_TRUE(base::Contains(event_observer.events(),
                             api::tab_groups::OnCreated::kEventName));
  Event* incognito_event =
      event_observer.events().at(api::tab_groups::OnCreated::kEventName).get();
  EXPECT_EQ(incognito_event->restrict_to_browser_context,
            incognito_browser->profile());
}

}  // namespace
}  // namespace extensions
