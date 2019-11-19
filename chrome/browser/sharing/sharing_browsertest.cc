// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_browsertest.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/sharing/sharing_device_registration_result.h"
#include "chrome/browser/sharing/sharing_device_source_sync.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/sessions_helper.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/sync/model_impl/client_tag_based_model_type_processor.h"
#include "components/sync_device_info/device_info.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/context_menu_data/media_type.h"

SharingBrowserTest::SharingBrowserTest()
    : SyncTest(TWO_CLIENT),
      scoped_testing_factory_installer_(
          base::BindRepeating(&gcm::FakeGCMProfileService::Build)) {}

SharingBrowserTest::~SharingBrowserTest() = default;

void SharingBrowserTest::SetUpOnMainThread() {
  SyncTest::SetUpOnMainThread();
  host_resolver()->AddRule("mock.http", "127.0.0.1");
}

void SharingBrowserTest::Init(
    sync_pb::SharingSpecificFields_EnabledFeatures first_device_feature,
    sync_pb::SharingSpecificFields_EnabledFeatures second_device_feature) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("mock.http", GetTestPageURL());
  ASSERT_TRUE(sessions_helper::OpenTab(0, url));

  web_contents_ = GetBrowser(0)->tab_strip_model()->GetWebContentsAt(0);
  ASSERT_TRUE(NavigateToURL(web_contents_, url));

  gcm_service_ = static_cast<gcm::FakeGCMProfileService*>(
      gcm::GCMProfileServiceFactory::GetForProfile(GetProfile(0)));
  gcm_service_->set_collect(true);

  sharing_service_ = SharingServiceFactory::GetForBrowserContext(GetProfile(0));

  SetUpDevices(first_device_feature, second_device_feature);
}

void SharingBrowserTest::SetUpDevices(
    sync_pb::SharingSpecificFields_EnabledFeatures first_device_feature,
    sync_pb::SharingSpecificFields_EnabledFeatures second_device_feature) {
  ASSERT_EQ(2u, GetSyncClients().size());

  RegisterDevice(0, first_device_feature);
  RegisterDevice(1, second_device_feature);

  syncer::DeviceInfoTracker* original_device_info_tracker =
      DeviceInfoSyncServiceFactory::GetForProfile(GetProfile(0))
          ->GetDeviceInfoTracker();
  std::vector<std::unique_ptr<syncer::DeviceInfo>> original_devices =
      original_device_info_tracker->GetAllDeviceInfo();
  ASSERT_EQ(2u, original_devices.size());

  for (size_t i = 0; i < original_devices.size(); i++)
    AddDeviceInfo(*original_devices[i], i);
  ASSERT_EQ(2, fake_device_info_tracker_.CountActiveDevices());
}

void SharingBrowserTest::RegisterDevice(
    int profile_index,
    sync_pb::SharingSpecificFields_EnabledFeatures feature) {
  SharingService* service =
      SharingServiceFactory::GetForBrowserContext(GetProfile(profile_index));
  static_cast<SharingDeviceSourceSync*>(service->GetDeviceSource())
      ->SetDeviceInfoTrackerForTesting(&fake_device_info_tracker_);

  base::RunLoop run_loop;
  service->RegisterDeviceInTesting(
      std::set<sync_pb::SharingSpecificFields_EnabledFeatures>{feature},
      base::BindLambdaForTesting([&](SharingDeviceRegistrationResult r) {
        ASSERT_EQ(SharingDeviceRegistrationResult::kSuccess, r);
        run_loop.Quit();
      }));
  run_loop.Run();
  ASSERT_TRUE(AwaitQuiescence());
}

void SharingBrowserTest::AddDeviceInfo(
    const syncer::DeviceInfo& original_device,
    int fake_device_id) {
  // The SharingInfo on the DeviceInfo will be empty. In this test we want the
  // SharingInfo to be read from SharingSyncPreference instead.
  base::Optional<syncer::DeviceInfo::SharingInfo> fake_sharing_info =
      base::nullopt;

  std::unique_ptr<syncer::DeviceInfo> fake_device =
      std::make_unique<syncer::DeviceInfo>(
          original_device.guid(),
          base::StrCat(
              {"testing_device_", base::NumberToString(fake_device_id)}),
          original_device.chrome_version(), original_device.sync_user_agent(),
          original_device.device_type(),
          original_device.signin_scoped_device_id(),
          base::SysInfo::HardwareInfo{
              "Google",
              base::StrCat({"model", base::NumberToString(fake_device_id)}),
              "serial_number"},
          original_device.last_updated_timestamp(),
          original_device.send_tab_to_self_receiving_enabled(),
          fake_sharing_info);
  fake_device_info_tracker_.Add(fake_device.get());
  device_infos_.push_back(std::move(fake_device));
}

std::unique_ptr<TestRenderViewContextMenu> SharingBrowserTest::InitContextMenu(
    const GURL& url,
    base::StringPiece link_text,
    base::StringPiece selection_text) {
  content::ContextMenuParams params;
  params.selection_text = base::ASCIIToUTF16(selection_text);
  params.media_type = blink::ContextMenuDataMediaType::kNone;
  params.unfiltered_link_url = url;
  params.link_url = url;
  params.src_url = url;
  params.link_text = base::ASCIIToUTF16(link_text);
  params.page_url = web_contents_->GetVisibleURL();
  params.source_type = ui::MenuSourceType::MENU_SOURCE_MOUSE;
#if defined(OS_MACOSX)
  params.writing_direction_default = 0;
  params.writing_direction_left_to_right = 0;
  params.writing_direction_right_to_left = 0;
#endif
  auto menu = std::make_unique<TestRenderViewContextMenu>(
      web_contents_->GetMainFrame(), params);
  menu->Init();
  return menu;
}

void SharingBrowserTest::CheckLastReceiver(
    const std::string& device_guid) const {
  auto target_info =
      sharing_service_->GetSyncPreferencesForTesting()->GetTargetInfo(
          device_guid);
  ASSERT_TRUE(target_info);
  EXPECT_EQ(target_info->fcm_token, gcm_service_->last_receiver_id());
}

chrome_browser_sharing::SharingMessage
SharingBrowserTest::GetLastSharingMessageSent() const {
  chrome_browser_sharing::SharingMessage sharing_message;
  sharing_message.ParseFromString(
      gcm_service_->last_web_push_message().payload);
  return sharing_message;
}

SharingService* SharingBrowserTest::sharing_service() const {
  return sharing_service_;
}

content::WebContents* SharingBrowserTest::web_contents() const {
  return web_contents_;
}
