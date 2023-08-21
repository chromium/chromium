// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/wm/wm_desks_private_events.h"
#include <memory>

#include "base/uuid.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "chrome/common/extensions/api/wm_desks_private.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/mock_render_process_host.h"
#include "extensions/browser/event_listener_map.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/test_event_router_observer.h"

namespace extensions {

class WMDesksPrivateEventsUnitTest : public ExtensionServiceTestWithInstall {
 public:
  WMDesksPrivateEventsUnitTest(const WMDesksPrivateEventsUnitTest&) = delete;
  WMDesksPrivateEventsUnitTest& operator=(const WMDesksPrivateEventsUnitTest&) =
      delete;

 protected:
  WMDesksPrivateEventsUnitTest() = default;
  ~WMDesksPrivateEventsUnitTest() override = default;

  content::RenderProcessHost* render_process_host() const {
    return render_process_host_.get();
  }

 private:
  // ExtensionServiceTestBase:
  void SetUp() override;
  void TearDown() override;

  std::unique_ptr<content::RenderProcessHost> render_process_host_;
};

void WMDesksPrivateEventsUnitTest::SetUp() {
  ExtensionServiceTestWithInstall::SetUp();

  InitializeExtensionService({});

  // Create API.
  EventRouterFactory::GetInstance()->SetTestingFactory(
      profile(), base::BindRepeating([](content::BrowserContext* profile) {
        return (std::unique_ptr<KeyedService>)std::make_unique<EventRouter>(
            profile, ExtensionPrefs::Get(profile));
      }));

  WMDesksPrivateEventsAPI::GetFactoryInstance()->SetTestingFactory(
      profile(), base::BindRepeating([](content::BrowserContext* context) {
        return (std::unique_ptr<KeyedService>)
            std::make_unique<WMDesksPrivateEventsAPI>(context);
      }));

  render_process_host_ =
      std::make_unique<content::MockRenderProcessHost>(profile());
}

void WMDesksPrivateEventsUnitTest::TearDown() {
  render_process_host_.reset();
  ExtensionServiceTestWithInstall::TearDown();
}

TEST_F(WMDesksPrivateEventsUnitTest, DispatchEventOnDeskAdded) {
  // Not instantiated in unit test. Explicitly trigger.
  WMDesksPrivateEventsAPI::Get(profile());
  // Add event listener.
  const ExtensionId listener_id = crx_file::id_util::GenerateId("listener");
  EventRouter* event_router = EventRouter::Get(profile());

  TestEventRouterObserver test_observer(event_router);
  auto* event_name = api::wm_desks_private::OnDeskAdded::kEventName;
  event_router->AddEventListener(event_name, render_process_host(),
                                 listener_id);

  // Trigger desk added event
  auto desk_1(base::Uuid::GenerateRandomV4());
  WMDesksPrivateEventsAPI::Get(profile())->desks_event_router()->OnDeskAdded(
      desk_1);

  const auto& event_map = test_observer.events();
  auto iter = event_map.find(event_name);
  ASSERT_FALSE(iter == event_map.end());
  ASSERT_EQ(2u, iter->second->event_args.size());
  EXPECT_EQ(desk_1.AsLowercaseString(), iter->second->event_args[0]);
}

TEST_F(WMDesksPrivateEventsUnitTest, DispatchEventOnDeskRemoved) {
  WMDesksPrivateEventsAPI::Get(profile());
  // Add event listener.
  const ExtensionId listener_id = crx_file::id_util::GenerateId("listener");
  EventRouter* event_router = EventRouter::Get(profile());

  TestEventRouterObserver test_observer(event_router);
  auto* event_name = api::wm_desks_private::OnDeskRemoved::kEventName;
  event_router->AddEventListener(event_name, render_process_host(),
                                 listener_id);

  // Trigger desk added event
  auto desk_1(base::Uuid::GenerateRandomV4());
  WMDesksPrivateEventsAPI::Get(profile())->desks_event_router()->OnDeskRemoved(
      desk_1);

  const auto& event_map = test_observer.events();
  auto iter = event_map.find(event_name);
  ASSERT_FALSE(iter == event_map.end());
  ASSERT_EQ(1u, iter->second->event_args.size());
  EXPECT_EQ(desk_1.AsLowercaseString(), iter->second->event_args[0]);
}

TEST_F(WMDesksPrivateEventsUnitTest, DispatchEventOnDeskSwitched) {
  WMDesksPrivateEventsAPI::Get(profile());
  // Add event listener.
  const ExtensionId listener_id = crx_file::id_util::GenerateId("listener");
  EventRouter* event_router = EventRouter::Get(profile());

  TestEventRouterObserver test_observer(event_router);
  auto* event_name = api::wm_desks_private::OnDeskSwitched::kEventName;
  event_router->AddEventListener(event_name, render_process_host(),
                                 listener_id);

  // Trigger desk added event
  auto desk_1(base::Uuid::GenerateRandomV4());
  auto desk_2(base::Uuid::GenerateRandomV4());
  WMDesksPrivateEventsAPI::Get(profile())->desks_event_router()->OnDeskSwitched(
      desk_1, desk_2);

  const auto& event_map = test_observer.events();
  auto iter = event_map.find(event_name);
  ASSERT_FALSE(iter == event_map.end());
  ASSERT_EQ(2u, iter->second->event_args.size());
  EXPECT_EQ(desk_1.AsLowercaseString(), iter->second->event_args[0]);
  EXPECT_EQ(desk_2.AsLowercaseString(), iter->second->event_args[1]);
}

TEST_F(WMDesksPrivateEventsUnitTest, DispatchEventOnDeskRemovalUndone) {
  WMDesksPrivateEventsAPI::Get(profile());
  // Add event listener.
  const ExtensionId listener_id = crx_file::id_util::GenerateId("listener");
  EventRouter* event_router = EventRouter::Get(profile());

  TestEventRouterObserver test_observer(event_router);
  auto* event_name = api::wm_desks_private::OnDeskAdded::kEventName;
  event_router->AddEventListener(event_name, render_process_host(),
                                 listener_id);

  // Trigger desk added event
  auto desk_1(base::Uuid::GenerateRandomV4());
  WMDesksPrivateEventsAPI::Get(profile())->desks_event_router()->OnDeskAdded(
      desk_1, true);

  const auto& event_map = test_observer.events();
  auto iter = event_map.find(event_name);
  ASSERT_FALSE(iter == event_map.end());
  ASSERT_EQ(2u, iter->second->event_args.size());
  EXPECT_EQ(desk_1.AsLowercaseString(), iter->second->event_args[0]);
}
}  // namespace extensions
