// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_list.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "build/build_config.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/send_tab_to_self_helper.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/history/core/browser/history_service.h"
#include "components/send_tab_to_self/page_context.h"
#include "components/send_tab_to_self/send_tab_to_self_bridge.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/local_device_info_provider.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace {

using send_tab_to_self_helper::GetFormFieldValueById;
using send_tab_to_self_helper::PopulateFormField;

}  // namespace

class TwoClientSendTabToSelfSyncTest
    : public SyncTest,
      public testing::WithParamInterface<SyncTest::SetupSyncMode> {
 public:
  TwoClientSendTabToSelfSyncTest() : SyncTest(TWO_CLIENT) {
    if (GetSetupSyncMode() == SetupSyncMode::kSyncTransportOnly) {
      scoped_feature_list_.InitAndEnableFeature(
          syncer::kReplaceSyncPromosWithSignInPromos);
    }
  }

  TwoClientSendTabToSelfSyncTest(const TwoClientSendTabToSelfSyncTest&) =
      delete;
  TwoClientSendTabToSelfSyncTest& operator=(
      const TwoClientSendTabToSelfSyncTest&) = delete;

  ~TwoClientSendTabToSelfSyncTest() override = default;

  SyncTest::SetupSyncMode GetSetupSyncMode() const override {
    return GetParam();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(,
                         TwoClientSendTabToSelfSyncTest,
                         GetSyncTestModes(),
                         testing::PrintToStringParamName());

IN_PROC_BROWSER_TEST_P(TwoClientSendTabToSelfSyncTest,
                       AddedUrlFoundWhenBothClientsAlreadySyncing) {
  const GURL kUrl("https://www.example.com");
  const base::Time kHistoryEntryTime = base::Time::Now();
  const std::string kTitle("example");
  const std::string kTargetDeviceSyncCacheGuid("target_device");

  ASSERT_TRUE(SetupSync());

  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(GetProfile(0),
                                           ServiceAccessType::EXPLICIT_ACCESS);
  history_service->AddPage(kUrl, kHistoryEntryTime, history::SOURCE_BROWSED);

  base::RunLoop run_loop;
  history_service->FlushForTest(run_loop.QuitClosure());
  run_loop.Run();

  send_tab_to_self::SendTabToSelfModel* model0 =
      SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0))
          ->GetSendTabToSelfModel();

  ASSERT_TRUE(model0->SendEntry(
      kUrl, kTitle, kTargetDeviceSyncCacheGuid, send_tab_to_self::PageContext(),
      send_tab_to_self::NavigationHistory(), base::DoNothing()));

  send_tab_to_self::SendTabToSelfSyncService* service1 =
      SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(1));

  EXPECT_TRUE(
      send_tab_to_self_helper::SendTabToSelfUrlChecker(service1, kUrl).Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientSendTabToSelfSyncTest,
                       ModelsMatchAfterAddWhenBothClientsAlreadySyncing) {
  const GURL kGurl0("https://www.example0.com");
  const std::string kTitle0("example0");
  const std::string kTargetDeviceSyncCacheGuid0("target0");

  const GURL kGurl1("https://www.example1.com");
  const std::string kTitle1("example1");
  const std::string kTargetDeviceSyncCacheGuid1("target1");

  const GURL kGurl2("https://www.example2.com");
  const std::string kTitle2("example2");
  const std::string kTargetDeviceSyncCacheGuid2("target2");

  ASSERT_TRUE(SetupSync());

  send_tab_to_self::SendTabToSelfModel* model0 =
      SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0))
          ->GetSendTabToSelfModel();

  ASSERT_TRUE(model0->SendEntry(kGurl0, kTitle0, kTargetDeviceSyncCacheGuid0,
                                send_tab_to_self::PageContext(),
                                send_tab_to_self::NavigationHistory(),
                                base::DoNothing()));

  ASSERT_TRUE(model0->SendEntry(kGurl1, kTitle1, kTargetDeviceSyncCacheGuid1,
                                send_tab_to_self::PageContext(),
                                send_tab_to_self::NavigationHistory(),
                                base::DoNothing()));

  ASSERT_TRUE(SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(1))
                  ->GetSendTabToSelfModel()
                  ->SendEntry(kGurl2, kTitle2, kTargetDeviceSyncCacheGuid2,
                              send_tab_to_self::PageContext(),
                              send_tab_to_self::NavigationHistory(),
                              base::DoNothing()));

  EXPECT_TRUE(send_tab_to_self_helper::SendTabToSelfModelEqualityChecker(
                  SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(1)),
                  SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0)))
                  .Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientSendTabToSelfSyncTest, IsActive) {
  ASSERT_TRUE(SetupSync());

  EXPECT_TRUE(send_tab_to_self_helper::SendTabToSelfActiveChecker(
                  SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0)))
                  .Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientSendTabToSelfSyncTest,
                       SendTabToSelfReceivingEnabled) {
  ASSERT_TRUE(SetupSync());

  DeviceInfoSyncServiceFactory::GetForProfile(GetProfile(1))
      ->GetDeviceInfoTracker()
      ->ForcePulseForTest();
  DeviceInfoSyncServiceFactory::GetForProfile(GetProfile(0))
      ->GetDeviceInfoTracker()
      ->ForcePulseForTest();

  ASSERT_TRUE(send_tab_to_self_helper::SendTabToSelfMultiDeviceActiveChecker(
                  DeviceInfoSyncServiceFactory::GetForProfile(GetProfile(1))
                      ->GetDeviceInfoTracker())
                  .Wait());

  std::vector<const syncer::DeviceInfo*> device_infos =
      DeviceInfoSyncServiceFactory::GetForProfile(GetProfile(1))
          ->GetDeviceInfoTracker()
          ->GetAllDeviceInfo();
  ASSERT_EQ(2u, device_infos.size());
  EXPECT_TRUE(device_infos[0]->send_tab_to_self_receiving_enabled());
  EXPECT_TRUE(device_infos[1]->send_tab_to_self_receiving_enabled());
}

IN_PROC_BROWSER_TEST_P(TwoClientSendTabToSelfSyncTest,
                       SendTabToSelfTargetDeviceInfoList) {
  ASSERT_TRUE(SetupSync());

  DeviceInfoSyncServiceFactory::GetForProfile(GetProfile(0))
      ->GetDeviceInfoTracker()
      ->ForcePulseForTest();
  DeviceInfoSyncServiceFactory::GetForProfile(GetProfile(1))
      ->GetDeviceInfoTracker()
      ->ForcePulseForTest();

  ASSERT_TRUE(send_tab_to_self_helper::SendTabToSelfMultiDeviceActiveChecker(
                  DeviceInfoSyncServiceFactory::GetForProfile(GetProfile(1))
                      ->GetDeviceInfoTracker())
                  .Wait());

  // Explicitly set the two profiles to have different client names to simulate
  // them being on different devices. Otherwise their device infos will get
  // deduped.
  // TODO(crbug.com/40200734): This is rather misleading. The
  // "device1"/"device2" strings below are never sent to the server, they just
  // ensure the local device name is different from the other entry. The same
  // string could even be used in both calls. The most robust test would be:
  // update the device info name and wait for the right value of
  // GetTargetDeviceInfoSortedList().
  static_cast<send_tab_to_self::SendTabToSelfBridge*>(
      SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0))
          ->GetSendTabToSelfModel())
      ->SetLocalDeviceNameForTest("device1");
  static_cast<send_tab_to_self::SendTabToSelfBridge*>(
      SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(1))
          ->GetSendTabToSelfModel())
      ->SetLocalDeviceNameForTest("device2");

  // Emulate a device info update to force the target device list to refresh.
  DeviceInfoSyncServiceFactory::GetForProfile(GetProfile(1))
      ->GetDeviceInfoTracker()
      ->ForcePulseForTest();
  DeviceInfoSyncServiceFactory::GetForProfile(GetProfile(0))
      ->GetDeviceInfoTracker()
      ->ForcePulseForTest();

  std::vector<send_tab_to_self::TargetDeviceInfo> profile1_target_devices =
      SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0))
          ->GetSendTabToSelfModel()
          ->GetTargetDeviceInfoSortedList();
  std::vector<send_tab_to_self::TargetDeviceInfo> profile2_target_devices =
      SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(1))
          ->GetSendTabToSelfModel()
          ->GetTargetDeviceInfoSortedList();

  EXPECT_EQ(1u, profile1_target_devices.size());
  EXPECT_EQ(1u, profile2_target_devices.size());
  EXPECT_TRUE(SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0))
                  ->GetSendTabToSelfModel()
                  ->HasValidTargetDevice());
  EXPECT_TRUE(SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(1))
                  ->GetSendTabToSelfModel()
                  ->HasValidTargetDevice());
}

IN_PROC_BROWSER_TEST_P(TwoClientSendTabToSelfSyncTest,
                       MarkOpenedWhenBothClientsAlreadySyncing) {
  const GURL kUrl("https://www.example.com");
  const base::Time kHistoryEntryTime = base::Time::Now();
  const std::string kTitle("example");
  const std::string kTargetDeviceSyncCacheGuid("target_device");

  ASSERT_TRUE(SetupSync());

  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(GetProfile(0),
                                           ServiceAccessType::EXPLICIT_ACCESS);
  history_service->AddPage(kUrl, kHistoryEntryTime, history::SOURCE_BROWSED);

  base::RunLoop run_loop;
  history_service->FlushForTest(run_loop.QuitClosure());
  run_loop.Run();

  send_tab_to_self::SendTabToSelfSyncService* service0 =
      SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0));

  send_tab_to_self::SendTabToSelfModel* model0 =
      service0->GetSendTabToSelfModel();

  ASSERT_TRUE(model0->SendEntry(
      kUrl, kTitle, kTargetDeviceSyncCacheGuid, send_tab_to_self::PageContext(),
      send_tab_to_self::NavigationHistory(), base::DoNothing()));

  send_tab_to_self::SendTabToSelfSyncService* service1 =
      SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(1));

  ASSERT_TRUE(
      send_tab_to_self_helper::SendTabToSelfUrlChecker(service1, kUrl).Wait());

  const std::string guid = model0->GetAllGuids()[0];

  service1->GetSendTabToSelfModel()->MarkEntryOpened(guid);

  EXPECT_TRUE(
      send_tab_to_self_helper::SendTabToSelfUrlOpenedChecker(service0, kUrl)
          .Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientSendTabToSelfSyncTest,
                       ShouldPropagateFormFields) {
  const std::string kName = "John";
  const std::string kEmail = "john@example.com";
  const GURL kUrl =
      embedded_test_server()->GetURL("/autofill/autofill_test_form.html");
  ASSERT_TRUE(SetupSync());

  // Client 0: Open tab and fill form.
  content::WebContents* sender_web_contents =
      chrome::AddAndReturnTabAt(GetBrowser(0), kUrl, -1, true);
  ASSERT_TRUE(content::WaitForLoadStop(sender_web_contents));

  // Wait for Autofill to cache the form fields.
  ASSERT_TRUE(
      send_tab_to_self_helper::AutofillFieldsSeenChecker(
          sender_web_contents, {{"NAME_FIRST", ""}, {"EMAIL_ADDRESS", ""}})
          .Wait());

  ASSERT_TRUE(PopulateFormField(sender_web_contents, "NAME_FIRST", kName));
  ASSERT_TRUE(PopulateFormField(sender_web_contents, "EMAIL_ADDRESS", kEmail));

  // Wait for Autofill to catch up with the values.
  ASSERT_TRUE(send_tab_to_self_helper::AutofillFieldsSeenChecker(
                  sender_web_contents,
                  {{"NAME_FIRST", kName}, {"EMAIL_ADDRESS", kEmail}})
                  .Wait());

  // Trigger sending.
  const std::string target_guid = GetCacheGuid(1);
  send_tab_to_self::PageContext context;
  context.form_field_info =
      send_tab_to_self::ExtractFormFieldsFromWebContents(sender_web_contents);
  SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0))
      ->GetSendTabToSelfModel()
      ->SendEntry(kUrl, "example", target_guid, context,
                  send_tab_to_self::NavigationHistory(), base::DoNothing());

  // Ensure receiver browser is active so notification is handled immediately,
  // as opposed to getting queued and executing during teardown.
  GetBrowser(1)->GetWindow()->Activate();

  // Client 1: Wait for entry and fill.
  send_tab_to_self::SendTabToSelfSyncService* service1 =
      SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(1));
  ASSERT_TRUE(
      send_tab_to_self_helper::SendTabToSelfUrlChecker(service1, kUrl).Wait());

  const send_tab_to_self::SendTabToSelfEntry* entry1 =
      service1->GetSendTabToSelfModel()->GetEntryByGUID(
          service1->GetSendTabToSelfModel()->GetAllGuids()[0]);
  ASSERT_NE(nullptr, entry1);

  // Mimic the tab being opened on client 1.
  content::WebContents* received_web_contents =
      chrome::AddAndReturnTabAt(GetBrowser(1), kUrl, -1, true);

  send_tab_to_self::FillWebContents(received_web_contents,
                                    url::Origin::Create(entry1->GetURL()),
                                    entry1->GetPageContext());

  // Wait for filling to complete.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return GetFormFieldValueById(received_web_contents, "NAME_FIRST") ==
               kName &&
           GetFormFieldValueById(received_web_contents, "EMAIL_ADDRESS") ==
               kEmail;
  }));
}

// Transport mode isn't really supported on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS)

class TwoClientSendTabToSelfTransportModeSyncTest : public SyncTest {
 public:
  TwoClientSendTabToSelfTransportModeSyncTest() : SyncTest(TWO_CLIENT) {}

  TwoClientSendTabToSelfTransportModeSyncTest(
      const TwoClientSendTabToSelfTransportModeSyncTest&) = delete;
  TwoClientSendTabToSelfTransportModeSyncTest& operator=(
      const TwoClientSendTabToSelfTransportModeSyncTest&) = delete;

  ~TwoClientSendTabToSelfTransportModeSyncTest() override = default;

  SyncTest::SetupSyncMode GetSetupSyncMode() const override {
    // This test specifically covers the interplay between a Sync-the-feature
    // client and a transport-mode client, so this method is not used (and
    // there's no need to parameterize).
    NOTREACHED();
  }
};

IN_PROC_BROWSER_TEST_F(TwoClientSendTabToSelfTransportModeSyncTest,
                       SignedInClientCanReceive) {
  ASSERT_TRUE(SetupClients());

  // Set up one client with Sync-the-feature and the other in transport mode.
  ASSERT_TRUE(GetClient(0)->SetupSync());
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureActive());

  ASSERT_TRUE(GetClient(1)->SignInNoWaitForCompletion());
  ASSERT_TRUE(GetClient(1)->AwaitSyncTransportActive());
  ASSERT_FALSE(GetSyncService(1)->IsSyncFeatureActive());

  DeviceInfoSyncServiceFactory::GetForProfile(GetProfile(1))
      ->GetDeviceInfoTracker()
      ->ForcePulseForTest();
  DeviceInfoSyncServiceFactory::GetForProfile(GetProfile(0))
      ->GetDeviceInfoTracker()
      ->ForcePulseForTest();

  ASSERT_TRUE(send_tab_to_self_helper::SendTabToSelfMultiDeviceActiveChecker(
                  DeviceInfoSyncServiceFactory::GetForProfile(GetProfile(1))
                      ->GetDeviceInfoTracker())
                  .Wait());

  std::vector<const syncer::DeviceInfo*> device_infos =
      DeviceInfoSyncServiceFactory::GetForProfile(GetProfile(1))
          ->GetDeviceInfoTracker()
          ->GetAllDeviceInfo();
  ASSERT_EQ(2u, device_infos.size());
  EXPECT_TRUE(device_infos[0]->send_tab_to_self_receiving_enabled());
  EXPECT_TRUE(device_infos[1]->send_tab_to_self_receiving_enabled());
}

#endif  // !BUILDFLAG(IS_CHROMEOS)
