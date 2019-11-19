// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_BROWSERTEST_H_
#define CHROME_BROWSER_SHARING_SHARING_BROWSERTEST_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/strings/string_piece_forward.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/sharing/sharing_service.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/gcm_driver/fake_gcm_profile_service.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/fake_device_info_tracker.h"
#include "url/gurl.h"

// Base test class for testing sharing features.
class SharingBrowserTest : public SyncTest {
 public:
  SharingBrowserTest();

  ~SharingBrowserTest() override;

  void SetUpOnMainThread() override;

  void Init(
      sync_pb::SharingSpecificFields_EnabledFeatures first_device_feature,
      sync_pb::SharingSpecificFields_EnabledFeatures second_device_feature);

  virtual std::string GetTestPageURL() const = 0;

  std::unique_ptr<TestRenderViewContextMenu> InitContextMenu(
      const GURL& url,
      base::StringPiece link_text,
      base::StringPiece selection_text);

  void CheckLastReceiver(const std::string& device_guid) const;

  chrome_browser_sharing::SharingMessage GetLastSharingMessageSent() const;

  SharingService* sharing_service() const;

  content::WebContents* web_contents() const;

 private:
  void SetUpDevices(
      sync_pb::SharingSpecificFields_EnabledFeatures first_device_feature,
      sync_pb::SharingSpecificFields_EnabledFeatures second_device_feature);

  void RegisterDevice(int profile_index,
                      sync_pb::SharingSpecificFields_EnabledFeatures feature);
  void AddDeviceInfo(const syncer::DeviceInfo& original_device,
                     int fake_device_id);

  gcm::GCMProfileServiceFactory::ScopedTestingFactoryInstaller
      scoped_testing_factory_installer_;
  gcm::FakeGCMProfileService* gcm_service_;
  content::WebContents* web_contents_;
  syncer::FakeDeviceInfoTracker fake_device_info_tracker_;
  std::vector<std::unique_ptr<syncer::DeviceInfo>> device_infos_;
  SharingService* sharing_service_;

  DISALLOW_COPY_AND_ASSIGN(SharingBrowserTest);
};

#endif  // CHROME_BROWSER_SHARING_SHARING_BROWSERTEST_H_
