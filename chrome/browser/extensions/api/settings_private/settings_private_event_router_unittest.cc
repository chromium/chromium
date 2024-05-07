// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/settings_private/settings_private_event_router.h"

#include "base/test/run_until.h"
#include "chrome/browser/extensions/api/settings_private/settings_private_event_router_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/process_map.h"
#include "extensions/browser/process_map_factory.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

// A fake that pretends that all contexts are WebUI.
class ProcessMapFake : public ProcessMap {
 public:
  explicit ProcessMapFake(content::BrowserContext* browser_context)
      : ProcessMap(browser_context) {}

  mojom::ContextType GetMostLikelyContextType(const Extension* extension,
                                              int process_id,
                                              const GURL* url) const override {
    return mojom::ContextType::kWebUi;
  }
};

std::unique_ptr<KeyedService> BuildEventRouter(
    content::BrowserContext* profile) {
  return std::make_unique<extensions::EventRouter>(profile, nullptr);
}

std::unique_ptr<KeyedService> BuildSettingsPrivateEventRouter(
    content::BrowserContext* profile) {
  return std::unique_ptr<KeyedService>(
      SettingsPrivateEventRouter::Create(profile));
}

std::unique_ptr<KeyedService> BuildProcessMap(
    content::BrowserContext* profile) {
  return std::make_unique<ProcessMapFake>(profile);
}

// Tracks event dispatches to a specific process.
class EventRouterObserver : public EventRouter::TestObserver {
 public:
  // Only counts events that match |process_id|.
  explicit EventRouterObserver(int process_id) : process_id_(process_id) {}

  void OnWillDispatchEvent(const Event& event) override {
    // Do nothing.
  }

  void OnDidDispatchEventToProcess(const Event& event,
                                   int process_id) override {
    if (process_id == process_id_) {
      ++dispatch_count;
    }
  }

  int dispatch_count = 0;
  const int process_id_;
};

class SettingsPrivateEventRouterTest : public testing::Test {
 public:
  SettingsPrivateEventRouterTest()
      : manager_(TestingBrowserProcess::GetGlobal()) {}
  void SetUp() override { ASSERT_TRUE(manager_.SetUp()); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager manager_;
};

// Tests that events from incognito profiles do not get routed to regular
// profiles. Regression test for https://crbug.com/1381219.
TEST_F(SettingsPrivateEventRouterTest, IncognitoEventRouting) {
  // Create a testing profile. Override relevant factories.
  TestingProfile* profile = manager_.CreateTestingProfile("test");
  EventRouterFactory::GetInstance()->SetTestingFactory(
      profile, base::BindRepeating(&BuildEventRouter));
  SettingsPrivateEventRouterFactory::GetInstance()->SetTestingFactory(
      profile, base::BindRepeating(&BuildSettingsPrivateEventRouter));
  ProcessMapFactory::GetInstance()->SetTestingFactory(
      profile, base::BindRepeating(&BuildProcessMap));

  // Create an otr profile. Override relevant factories.
  Profile::OTRProfileID otr_id = Profile::OTRProfileID::PrimaryID();
  Profile* otr_profile =
      profile->GetOffTheRecordProfile(otr_id, /*create_if_needed=*/true);
  EventRouterFactory::GetInstance()->SetTestingFactory(
      otr_profile, base::BindRepeating(&BuildEventRouter));
  SettingsPrivateEventRouterFactory::GetInstance()->SetTestingFactory(
      otr_profile, base::BindRepeating(&BuildSettingsPrivateEventRouter));
  ProcessMapFactory::GetInstance()->SetTestingFactory(
      otr_profile, base::BindRepeating(&BuildProcessMap));

  // Create the event routers.
  EventRouter* regular_event_router =
      EventRouterFactory::GetInstance()->GetForBrowserContext(profile);
  EventRouter* otr_event_router =
      EventRouterFactory::GetInstance()->GetForBrowserContext(otr_profile);

  // Today, EventRouter instances are shared between on- and off-the-record
  // profile instances. We separate them into variables here, since the
  // SettingsPrivateEventRouter shouldn't necessarily know about that or
  // care.
  EXPECT_EQ(regular_event_router, otr_event_router);

  // Create the special routers for settingsPrivate.
  ASSERT_TRUE(SettingsPrivateEventRouterFactory::GetForProfile(profile));
  ASSERT_TRUE(SettingsPrivateEventRouterFactory::GetForProfile(otr_profile));

  // Create some mock rphs.
  content::MockRenderProcessHost regular_rph(profile);
  content::MockRenderProcessHost otr_rph(otr_profile);

  // Add event listeners, as if we had created two real WebUIs, one in a regular
  // profile and one in an otr profile. Note that the string chrome://settings
  // is hardcoded into the api permissions of settingsPrivate.
  GURL kDummyURL("chrome://settings");
  regular_event_router->AddEventListenerForURL(
      api::settings_private::OnPrefsChanged::kEventName, &regular_rph,
      kDummyURL);
  otr_event_router->AddEventListenerForURL(
      api::settings_private::OnPrefsChanged::kEventName, &otr_rph, kDummyURL);

  // Hook up some test observers
  EventRouterObserver regular_counter(regular_rph.GetID());
  regular_event_router->AddObserverForTesting(&regular_counter);
  EventRouterObserver otr_counter(otr_rph.GetID());
  otr_event_router->AddObserverForTesting(&otr_counter);

  EXPECT_EQ(0, regular_counter.dispatch_count);
  EXPECT_EQ(0, otr_counter.dispatch_count);

  // Setting an otr pref should not trigger the normal observer.
  otr_profile->GetPrefs()->SetBoolean(prefs::kPromptForDownload, true);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return otr_counter.dispatch_count == 1; }));
  EXPECT_EQ(0, regular_counter.dispatch_count);
  EXPECT_EQ(1, otr_counter.dispatch_count);

  // Setting a regular pref should not trigger the otr observer.
  profile->GetPrefs()->SetBoolean(prefs::kPromptForDownload, true);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return regular_counter.dispatch_count == 1; }));
  EXPECT_EQ(1, regular_counter.dispatch_count);
  EXPECT_EQ(1, otr_counter.dispatch_count);
}

}  // namespace
}  // namespace extensions
