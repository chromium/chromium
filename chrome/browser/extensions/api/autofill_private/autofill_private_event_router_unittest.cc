// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/autofill_private/autofill_private_event_router.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autofill/autofill_entity_data_manager_factory.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/extensions/api/autofill_private/autofill_private_event_router_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/common/extensions/api/autofill_private.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_manager/test_personal_data_manager.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_test_helper.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/common/child_process_id.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/process_map.h"
#include "extensions/browser/process_map_factory.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "url/gurl.h"

namespace extensions {
namespace {

constexpr char kSettingsUrl[] = "chrome://settings";

// Pretends that all contexts are WebUI so `EventRouter` dispatches
// `autofillPrivate` events to `chrome://settings` listeners in unit tests.
class ProcessMapFake : public ProcessMap {
 public:
  explicit ProcessMapFake(content::BrowserContext* browser_context)
      : ProcessMap(browser_context) {}

  mojom::ContextType GetMostLikelyContextType(
      const Extension* extension,
      content::ChildProcessId process_id,
      const GURL* url) const override {
    return mojom::ContextType::kWebUi;
  }
};

// Scoped `EventRouter::TestObserver` recording both `BroadcastEvent` calls
// (even when zero listeners exist) and per-process dispatches.
class ScopedEventRouterTestObserver : public EventRouter::TestObserver {
 public:
  explicit ScopedEventRouterTestObserver(EventRouter& event_router)
      : event_router_(event_router) {
    event_router_->AddObserverForTesting(this);
  }

  ~ScopedEventRouterTestObserver() override {
    event_router_->RemoveObserverForTesting(this);
  }

  void OnWillDispatchEvent(const Event& event) override {}

  void OnWillBroadcastEvent(const Event& event) override {
    ++broadcast_counts_[event.event_name];
  }

  void OnDidDispatchEventToProcess(const Event& event,
                                   int process_id) override {
    ++dispatch_counts_[{event.event_name, process_id}];
  }

  int GetBroadcastCount(const std::string& event_name) const {
    const auto it = broadcast_counts_.find(event_name);
    return it != broadcast_counts_.end() ? it->second : 0;
  }

  int GetDispatchCount(const std::string& event_name, int process_id) const {
    const auto it = dispatch_counts_.find({event_name, process_id});
    return it != dispatch_counts_.end() ? it->second : 0;
  }

 private:
  const raw_ref<EventRouter> event_router_;
  // Keyed by event name.
  absl::flat_hash_map<std::string, int> broadcast_counts_;
  // Keyed by (event name, render process ID).
  absl::flat_hash_map<std::pair<std::string, int>, int> dispatch_counts_;
};

std::unique_ptr<KeyedService> BuildEventRouter(
    content::BrowserContext* context) {
  return std::make_unique<EventRouter>(context, /*extension_prefs=*/nullptr);
}

std::unique_ptr<KeyedService> BuildProcessMap(
    content::BrowserContext* context) {
  return std::make_unique<ProcessMapFake>(context);
}

std::unique_ptr<KeyedService> BuildTestPersonalDataManager(
    content::BrowserContext* context) {
  auto pdm = std::make_unique<autofill::TestPersonalDataManager>();
  pdm->SetPrefService(Profile::FromBrowserContext(context)->GetPrefs());
  return pdm;
}

std::unique_ptr<KeyedService> BuildTestSyncService(
    content::BrowserContext* context) {
  return std::make_unique<syncer::TestSyncService>();
}

std::unique_ptr<KeyedService> BuildEntityDataManager(
    scoped_refptr<autofill::AutofillWebDataService> webdata_service,
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<autofill::EntityDataManager>(
      profile->GetPrefs(), IdentityManagerFactory::GetForProfile(profile),
      SyncServiceFactory::GetForProfile(profile), std::move(webdata_service),
      /*history_service=*/nullptr,
      /*pcontext_manager=*/nullptr,
      /*strike_database=*/nullptr, autofill::GeoIpCountryCode("US"));
}

class AutofillPrivateEventRouterTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(profile_manager_.SetUp()); }

  TestingProfile* CreateProfileWithServices(const std::string& name) {
    TestingProfile* profile = profile_manager_.CreateTestingProfile(
        name,
        {TestingProfile::TestingFactory{EventRouterFactory::GetInstance(),
                                        base::BindRepeating(&BuildEventRouter)},
         TestingProfile::TestingFactory{ProcessMapFactory::GetInstance(),
                                        base::BindRepeating(&BuildProcessMap)},
         TestingProfile::TestingFactory{
             autofill::PersonalDataManagerFactory::GetInstance(),
             base::BindRepeating(&BuildTestPersonalDataManager)},
         TestingProfile::TestingFactory{
             SyncServiceFactory::GetInstance(),
             base::BindRepeating(&BuildTestSyncService)},
         TestingProfile::TestingFactory{
             autofill::AutofillEntityDataManagerFactory::GetInstance(),
             base::BindRepeating(&BuildEntityDataManager,
                                 webdata_helper_.autofill_webdata_service())}});
    // Flush `EntityDataManager`'s initial asynchronous
    // `LoadEntitiesFromDatabase()` request so that its notification is
    // delivered now rather than during a later test step.
    webdata_helper_.WaitUntilIdle();
    return profile;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  autofill::AutofillWebDataServiceTestHelper webdata_helper_{
      std::make_unique<autofill::EntityTable>()};
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
};

class AutofillPrivateEventRouterObserveOnDemandTest
    : public AutofillPrivateEventRouterTest,
      public testing::WithParamInterface<bool> {
 public:
  AutofillPrivateEventRouterObserveOnDemandTest() {
    scoped_feature_list_.InitWithFeatureState(
        kAutofillPrivateEventRouterObserveOnDemand, IsObserveOnDemandEnabled());
  }

  bool IsObserveOnDemandEnabled() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(All,
                         AutofillPrivateEventRouterObserveOnDemandTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "Enabled" : "Disabled";
                         });

// Verifies that without listeners, `AutofillPrivateEventRouter` observes
// `SyncService` and broadcasts on every `PersonalDataManager`, `SyncService`
// and `EntityDataManager` notification only when
// `kAutofillPrivateEventRouterObserveOnDemand` is disabled.
TEST_P(AutofillPrivateEventRouterObserveOnDemandTest, NoListeners) {
  TestingProfile* profile = CreateProfileWithServices("no_listeners");
  AutofillPrivateEventRouter* router =
      AutofillPrivateEventRouterFactory::GetForProfile(profile);
  ASSERT_NE(router, nullptr);

  EventRouter& event_router =
      CHECK_DEREF(EventRouterFactory::GetForBrowserContext(profile));
  auto& pdm = static_cast<autofill::TestPersonalDataManager&>(CHECK_DEREF(
      autofill::PersonalDataManagerFactory::GetForBrowserContext(profile)));
  auto& sync_service = static_cast<syncer::TestSyncService&>(
      CHECK_DEREF(SyncServiceFactory::GetForProfile(profile)));
  autofill::EntityDataManager& entity_data_manager = CHECK_DEREF(
      autofill::AutofillEntityDataManagerFactory::GetForProfile(profile));

  EXPECT_FALSE(event_router.HasEventListener(
      api::autofill_private::OnPersonalDataChanged::kEventName));
  EXPECT_FALSE(event_router.HasEventListener(
      api::autofill_private::OnEntityInstancesChanged::kEventName));
  EXPECT_EQ(sync_service.HasObserver(router), !IsObserveOnDemandEnabled());

  ScopedEventRouterTestObserver observer(event_router);
  content::MockRenderProcessHost rph(profile);

  pdm.NotifyPersonalDataObserver();
  sync_service.FireStateChanged();
  entity_data_manager.NotifyEntityInstancesChanged();

  EXPECT_EQ(observer.GetBroadcastCount(
                api::autofill_private::OnPersonalDataChanged::kEventName),
            IsObserveOnDemandEnabled() ? 0 : 2);
  EXPECT_EQ(observer.GetBroadcastCount(
                api::autofill_private::OnEntityInstancesChanged::kEventName),
            IsObserveOnDemandEnabled() ? 0 : 1);
  EXPECT_EQ(observer.GetDispatchCount(
                api::autofill_private::OnPersonalDataChanged::kEventName,
                rph.GetDeprecatedID()),
            0);
  EXPECT_EQ(observer.GetDispatchCount(
                api::autofill_private::OnEntityInstancesChanged::kEventName,
                rph.GetDeprecatedID()),
            0);
}

// Verifies independent per-event observation gating, multiple-listener
// idempotency, and clean detachment when the last listener is removed.
TEST_F(AutofillPrivateEventRouterTest,
       ObservesPersonalDataAndSyncIndependentlyOfEntityInstances) {
  scoped_feature_list_.InitAndEnableFeature(
      kAutofillPrivateEventRouterObserveOnDemand);

  TestingProfile* profile = CreateProfileWithServices("enabled_independent");
  AutofillPrivateEventRouter* router =
      AutofillPrivateEventRouterFactory::GetForProfile(profile);
  ASSERT_NE(router, nullptr);

  EventRouter& event_router =
      CHECK_DEREF(EventRouterFactory::GetForBrowserContext(profile));
  auto& pdm = static_cast<autofill::TestPersonalDataManager&>(CHECK_DEREF(
      autofill::PersonalDataManagerFactory::GetForBrowserContext(profile)));
  auto& sync_service = static_cast<syncer::TestSyncService&>(
      CHECK_DEREF(SyncServiceFactory::GetForProfile(profile)));
  autofill::EntityDataManager& entity_data_manager = CHECK_DEREF(
      autofill::AutofillEntityDataManagerFactory::GetForProfile(profile));

  ScopedEventRouterTestObserver observer(event_router);
  content::MockRenderProcessHost rph_1(profile);
  content::MockRenderProcessHost rph_2(profile);
  const GURL settings_url(kSettingsUrl);

  // 1. Adding an `onPersonalDataChanged` listener attaches
  // `PersonalDataManager` and `SyncService` observers (without broadcasting on
  // attach), while leaving `EntityDataManager` unobserved.
  event_router.AddEventListenerForURL(
      api::autofill_private::OnPersonalDataChanged::kEventName, &rph_1,
      settings_url);
  EXPECT_TRUE(sync_service.HasObserver(router));
  EXPECT_EQ(observer.GetBroadcastCount(
                api::autofill_private::OnPersonalDataChanged::kEventName),
            0);

  pdm.NotifyPersonalDataObserver();
  sync_service.FireStateChanged();
  entity_data_manager.NotifyEntityInstancesChanged();

  EXPECT_EQ(observer.GetBroadcastCount(
                api::autofill_private::OnPersonalDataChanged::kEventName),
            2);
  EXPECT_EQ(observer.GetDispatchCount(
                api::autofill_private::OnPersonalDataChanged::kEventName,
                rph_1.GetDeprecatedID()),
            2);
  EXPECT_EQ(observer.GetBroadcastCount(
                api::autofill_private::OnEntityInstancesChanged::kEventName),
            0);

  // 2. Adding a second `onPersonalDataChanged` listener is idempotent; removing
  // the first keeps observation active until the last listener detaches.
  event_router.AddEventListenerForURL(
      api::autofill_private::OnPersonalDataChanged::kEventName, &rph_2,
      settings_url);
  event_router.RemoveEventListenerForURL(
      api::autofill_private::OnPersonalDataChanged::kEventName, &rph_1,
      settings_url);
  EXPECT_TRUE(sync_service.HasObserver(router));

  sync_service.FireStateChanged();
  EXPECT_EQ(observer.GetBroadcastCount(
                api::autofill_private::OnPersonalDataChanged::kEventName),
            3);
  EXPECT_EQ(observer.GetDispatchCount(
                api::autofill_private::OnPersonalDataChanged::kEventName,
                rph_1.GetDeprecatedID()),
            2);
  EXPECT_EQ(observer.GetDispatchCount(
                api::autofill_private::OnPersonalDataChanged::kEventName,
                rph_2.GetDeprecatedID()),
            1);

  event_router.RemoveEventListenerForURL(
      api::autofill_private::OnPersonalDataChanged::kEventName, &rph_2,
      settings_url);
  EXPECT_FALSE(sync_service.HasObserver(router));

  pdm.NotifyPersonalDataObserver();
  sync_service.FireStateChanged();
  EXPECT_EQ(observer.GetBroadcastCount(
                api::autofill_private::OnPersonalDataChanged::kEventName),
            3);

  // 3. Adding an `onEntityInstancesChanged` listener attaches only
  // `EntityDataManager` without observing `PersonalDataManager` or
  // `SyncService`.
  event_router.AddEventListenerForURL(
      api::autofill_private::OnEntityInstancesChanged::kEventName, &rph_1,
      settings_url);
  EXPECT_FALSE(sync_service.HasObserver(router));

  pdm.NotifyPersonalDataObserver();
  sync_service.FireStateChanged();
  entity_data_manager.NotifyEntityInstancesChanged();

  EXPECT_EQ(observer.GetBroadcastCount(
                api::autofill_private::OnPersonalDataChanged::kEventName),
            3);
  EXPECT_EQ(observer.GetBroadcastCount(
                api::autofill_private::OnEntityInstancesChanged::kEventName),
            1);
  EXPECT_EQ(observer.GetDispatchCount(
                api::autofill_private::OnEntityInstancesChanged::kEventName,
                rph_1.GetDeprecatedID()),
            1);

  event_router.RemoveEventListenerForURL(
      api::autofill_private::OnEntityInstancesChanged::kEventName, &rph_1,
      settings_url);
  entity_data_manager.NotifyEntityInstancesChanged();
  EXPECT_EQ(observer.GetBroadcastCount(
                api::autofill_private::OnEntityInstancesChanged::kEventName),
            1);
}

// Verifies that constructing and shutting down `AutofillPrivateEventRouter`
// when `EventRouter::Get(profile)` is null is safe.
TEST_P(AutofillPrivateEventRouterObserveOnDemandTest,
       ShutdownWithNullEventRouter) {
  TestingProfile* profile =
      profile_manager_.CreateTestingProfile("null_router");
  ASSERT_EQ(EventRouter::Get(profile), nullptr);

  AutofillPrivateEventRouter* router =
      AutofillPrivateEventRouterFactory::GetForProfile(profile);
  ASSERT_NE(router, nullptr);

  profile_manager_.DeleteTestingProfile("null_router");
}

}  // namespace
}  // namespace extensions
