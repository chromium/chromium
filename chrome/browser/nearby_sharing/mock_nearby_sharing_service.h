// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_MOCK_NEARBY_SHARING_SERVICE_H_
#define CHROME_BROWSER_NEARBY_SHARING_MOCK_NEARBY_SHARING_SERVICE_H_

#include "chrome/browser/nearby_sharing/nearby_sharing_service.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockNearbySharingService : public NearbySharingService {
 public:
  MockNearbySharingService();
  ~MockNearbySharingService() override;

  // NearbySharingService:
  MOCK_METHOD(void, AddObserver, (NearbySharingService::Observer*), (override));
  MOCK_METHOD(void,
              RemoveObserver,
              (NearbySharingService::Observer*),
              (override));
  MOCK_METHOD(bool, HasObserver, (NearbySharingService::Observer*), (override));
  MOCK_METHOD(StatusCodes,
              RegisterSendSurface,
              (TransferUpdateCallback*,
               ShareTargetDiscoveredCallback*,
               SendSurfaceState),
              (override));
  MOCK_METHOD(StatusCodes,
              UnregisterSendSurface,
              (TransferUpdateCallback*, ShareTargetDiscoveredCallback*),
              (override));
  MOCK_METHOD(StatusCodes,
              RegisterReceiveSurface,
              (TransferUpdateCallback*, ReceiveSurfaceState),
              (override));
  MOCK_METHOD(StatusCodes,
              UnregisterReceiveSurface,
              (TransferUpdateCallback*),
              (override));
  MOCK_METHOD(StatusCodes, ClearForegroundReceiveSurfaces, (), (override));
  MOCK_METHOD(bool, IsInHighVisibility, (), (const override));
  MOCK_METHOD(bool, IsTransferring, (), (const override));
  MOCK_METHOD(bool, IsSendingFile, (), (const override));
  MOCK_METHOD(bool, IsReceivingFile, (), (const override));
  MOCK_METHOD(bool, IsConnecting, (), (const override));
  MOCK_METHOD(bool, IsScanning, (), (const override));
  MOCK_METHOD(StatusCodes,
              SendAttachments,
              (const ShareTarget&, std::vector<std::unique_ptr<Attachment>>),
              (override));
  MOCK_METHOD(void,
              Accept,
              (const ShareTarget&, StatusCodesCallback),
              (override));
  MOCK_METHOD(void,
              Reject,
              (const ShareTarget&, StatusCodesCallback),
              (override));
  MOCK_METHOD(void,
              Cancel,
              (const ShareTarget&, StatusCodesCallback),
              (override));
  MOCK_METHOD(bool,
              DidLocalUserCancelTransfer,
              (const ShareTarget&),
              (override));
  MOCK_METHOD(void,
              Open,
              (const ShareTarget&, StatusCodesCallback),
              (override));
  MOCK_METHOD(void, OpenURL, (GURL), (override));
  MOCK_METHOD(void,
              SetArcTransferCleanupCallback,
              (base::OnceCallback<void()>),
              (override));
  MOCK_METHOD(NearbyNotificationDelegate*,
              GetNotificationDelegate,
              (const std::string&),
              (override));
  MOCK_METHOD(NearbyNotificationManager*,
              GetNotificationManager,
              (),
              (override));
  MOCK_METHOD(void, RecordFastInitiationNotificationUsage, (bool), (override));
  MOCK_METHOD(NearbyShareSettings*, GetSettings, (), (override));
  MOCK_METHOD(NearbyShareHttpNotifier*, GetHttpNotifier, (), (override));
  MOCK_METHOD(NearbyShareLocalDeviceDataManager*,
              GetLocalDeviceDataManager,
              (),
              (override));
  MOCK_METHOD(NearbyShareContactManager*, GetContactManager, (), (override));
  MOCK_METHOD(NearbyShareCertificateManager*,
              GetCertificateManager,
              (),
              (override));
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_MOCK_NEARBY_SHARING_SERVICE_H_
