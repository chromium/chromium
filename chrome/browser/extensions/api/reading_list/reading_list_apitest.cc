// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/reading_list/reading_list_model_factory.h"
#include "chrome/common/extensions/api/reading_list.h"
#include "components/reading_list/core/reading_list_model.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/test_event_router_observer.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {

using ReadingListApiTest = ExtensionApiTest;

IN_PROC_BROWSER_TEST_F(ReadingListApiTest, TestReadingListWorks) {
  ASSERT_TRUE(RunExtensionTest("reading_list")) << message_;
}

// TODO(crbug.com/40931607): Re-enable this test
#if BUILDFLAG(IS_MAC)
#define MAYBE_TestReadingListEventsAcrossProfiles \
  DISABLED_TestReadingListEventsAcrossProfiles
#else
#define MAYBE_TestReadingListEventsAcrossProfiles \
  TestReadingListEventsAcrossProfiles
#endif  // BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(ReadingListApiTest,
                       MAYBE_TestReadingListEventsAcrossProfiles) {
  // The EventRouter is shared between on- and off-the-record profiles, so
  // this observer will catch events for each.
  TestEventRouterObserver event_observer(EventRouter::Get(profile()));

  // Add a Reading List entry in a normal profile.
  ReadingListModel* const reading_list_model =
      ReadingListModelFactory::GetForBrowserContext(profile());
  reading_list_model->AddOrReplaceEntry(
      GURL("https://www.example.com"), "example of title",
      reading_list::EntrySource::ADDED_VIA_CURRENT_APP,
      /*estimated_read_time=*/std::nullopt,
      /*creation_time=*/std::nullopt);

  ASSERT_TRUE(base::Contains(event_observer.events(),
                             api::reading_list::OnEntryAdded::kEventName));
  Event* normal_event = event_observer.events()
                            .at(api::reading_list::OnEntryAdded::kEventName)
                            .get();
  EXPECT_EQ(normal_event->restrict_to_browser_context, profile());

  event_observer.ClearEvents();
  ASSERT_FALSE(base::Contains(event_observer.events(),
                              api::reading_list::OnEntryAdded::kEventName));

  Profile* incognito_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);

  // Add a Reading List entry in an off-the-record profile.
  ReadingListModel* const incognito_reading_list_model =
      ReadingListModelFactory::GetForBrowserContext(incognito_profile);
  incognito_reading_list_model->AddOrReplaceEntry(
      GURL("https://www.example.com"), "example of title",
      reading_list::EntrySource::ADDED_VIA_CURRENT_APP,
      /*estimated_read_time=*/std::nullopt,
      /*creation_time=*/std::nullopt);

  ASSERT_TRUE(base::Contains(event_observer.events(),
                             api::reading_list::OnEntryAdded::kEventName));
  Event* incognito_event = event_observer.events()
                               .at(api::reading_list::OnEntryAdded::kEventName)
                               .get();
  EXPECT_EQ(incognito_event->restrict_to_browser_context, incognito_profile);
}

}  // namespace

}  // namespace extensions
