// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/send_tab_to_self/android_notification_handler.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model_test_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/send_tab_to_self/fake_send_tab_to_self_model.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/page_context.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/stub_send_tab_to_self_sync_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace send_tab_to_self {
namespace {

constexpr char kExampleUrl[] = "https://www.example.com/";
constexpr char kDeviceId[] = "device_id";

std::unique_ptr<KeyedService> BuildStubSendTabToSelfSyncService(
    content::BrowserContext* context) {
  return std::make_unique<StubSendTabToSelfSyncService>();
}

// Test double of AndroidNotificationHandler that overrides JNI notification
// calls to avoid hitting the JVM, and records which GUIDs were shown or hidden.
class FakeAndroidNotificationHandler : public AndroidNotificationHandler {
 public:
  using AndroidNotificationHandler::AndroidNotificationHandler;

  const std::vector<std::string>& shown_guids() const { return shown_guids_; }
  const std::vector<std::string>& hidden_guids() const { return hidden_guids_; }

  void ClearRecords() {
    shown_guids_.clear();
    hidden_guids_.clear();
  }

 protected:
  void ShowNotification(const SendTabToSelfEntry& entry) override {
    shown_guids_.push_back(entry.GetGUID());
  }

  void HideNotification(const std::string& guid) override {
    hidden_guids_.push_back(guid);
  }

 private:
  std::vector<std::string> shown_guids_;
  std::vector<std::string> hidden_guids_;
};

class AndroidNotificationHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  AndroidNotificationHandlerTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~AndroidNotificationHandlerTest() override = default;

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return {TestingProfile::TestingFactory{
        SendTabToSelfSyncServiceFactory::GetInstance(),
        base::BindRepeating(&BuildStubSendTabToSelfSyncService)}};
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    tab_model_ = std::make_unique<TestTabModel>(profile());
    tab_model_->SetWebContentsList({web_contents()});
    tab_model_->SetIsActiveModel(true);

    handler_ = std::make_unique<FakeAndroidNotificationHandler>(model());
    model()->SetLocalCacheGuid(kDeviceId);
  }

  void TearDown() override {
    handler_.reset();
    tab_model_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  FakeSendTabToSelfModel* model() {
    return static_cast<StubSendTabToSelfSyncService*>(
               SendTabToSelfSyncServiceFactory::GetForProfile(profile()))
        ->GetFakeSendTabToSelfModel();
  }

  FakeAndroidNotificationHandler* handler() { return handler_.get(); }

  void WaitForEntryOpened(const std::string& guid) {
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return model()->GetEntryByGUID(guid)->IsOpened(); }));
  }

  void WaitForNotificationShown() {
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return !handler()->shown_guids().empty(); }));
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_{kSendTabToSelfAutoOpen};
  std::unique_ptr<TestTabModel> tab_model_;
  std::unique_ptr<FakeAndroidNotificationHandler> handler_;
};

TEST_F(AndroidNotificationHandlerTest,
       ShouldAutoOpenNewEntriesInBackgroundIfActive) {
  TabModelList::AddTabModel(tab_model_.get());

  // Add a remote entry to the model first.
  const SendTabToSelfEntry* entry =
      model()->AddEntryRemotely(GURL(kExampleUrl), "Title", kDeviceId,
                                PageContext(), NavigationHistory());
  const std::string guid = entry->GetGUID();

  // Trigger the addition of a new entry using the public ReceivingUiHandler
  // interface.
  static_cast<ReceivingUiHandler*>(handler())->DisplayNewEntries({entry});

  // Allow UI thread posted tasks to execute.
  WaitForEntryOpened(guid);

  // Verification:
  // 1. Notification is NOT shown.
  EXPECT_TRUE(handler()->shown_guids().empty());
  // 2. Model is notified to mark the entry as opened.
  EXPECT_TRUE(model()->GetEntryByGUID(guid)->IsOpened());

  TabModelList::RemoveTabModel(tab_model_.get());
}

TEST_F(AndroidNotificationHandlerTest, ShouldNotAutoOpenNewEntriesIfNotActive) {
  // Do NOT add tab_model_ to TabModelList (simulating Chrome running
  // in background or not started).
  const SendTabToSelfEntry* entry =
      model()->AddEntryRemotely(GURL(kExampleUrl), "Title", kDeviceId,
                                PageContext(), NavigationHistory());
  const std::string guid = entry->GetGUID();

  static_cast<ReceivingUiHandler*>(handler())->DisplayNewEntries({entry});
  WaitForNotificationShown();

  // Verification:
  // 1. Notification IS shown (falls back gracefully).
  EXPECT_THAT(handler()->shown_guids(), testing::ElementsAre(guid));
  // 2. Model is NOT notified to mark the entry as opened yet.
  EXPECT_FALSE(model()->GetEntryByGUID(guid)->IsOpened());
}

TEST_F(AndroidNotificationHandlerTest,
       ShouldAutoOpenPendingEntriesInBackgroundOnActivation) {
  // Simulate multiple unread entries stored in the model.
  const SendTabToSelfEntry* entry1 =
      model()->AddEntryRemotely(GURL("https://www.google.com/"), "Google",
                                kDeviceId, PageContext(), NavigationHistory());
  const SendTabToSelfEntry* entry2 =
      model()->AddEntryRemotely(GURL("https://www.youtube.com/"), "YouTube",
                                kDeviceId, PageContext(), NavigationHistory());

  const std::string guid1 = entry1->GetGUID();
  const std::string guid2 = entry2->GetGUID();

  // Adding the tab model triggers OnTabModelAdded which executes auto-open on
  // all unread entries.
  TabModelList::AddTabModel(tab_model_.get());

  // Allow tasks to execute.
  WaitForEntryOpened(guid1);
  WaitForEntryOpened(guid2);

  // Verification:
  // 1. Both entries are marked as opened.
  EXPECT_TRUE(model()->GetEntryByGUID(guid1)->IsOpened());
  EXPECT_TRUE(model()->GetEntryByGUID(guid2)->IsOpened());
  // 2. Both notifications are hidden/dismissed.
  EXPECT_THAT(handler()->hidden_guids(),
              testing::UnorderedElementsAre(guid1, guid2));

  TabModelList::RemoveTabModel(tab_model_.get());
}

TEST_F(AndroidNotificationHandlerTest,
       ShouldNotAutoOpenInOffTheRecordTabModel) {
  TestTabModel otr_tab_model(
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true));
  otr_tab_model.SetWebContentsList({web_contents()});
  otr_tab_model.SetIsActiveModel(true);

  TabModelList::AddTabModel(&otr_tab_model);

  const SendTabToSelfEntry* entry =
      model()->AddEntryRemotely(GURL(kExampleUrl), "Title", kDeviceId,
                                PageContext(), NavigationHistory());
  const std::string guid = entry->GetGUID();

  static_cast<ReceivingUiHandler*>(handler())->DisplayNewEntries({entry});
  WaitForNotificationShown();

  // Verification: Notification IS shown because OTR model is ignored.
  EXPECT_THAT(handler()->shown_guids(), testing::ElementsAre(guid));
  EXPECT_FALSE(model()->GetEntryByGUID(guid)->IsOpened());

  TabModelList::RemoveTabModel(&otr_tab_model);
}

}  // namespace
}  // namespace send_tab_to_self
