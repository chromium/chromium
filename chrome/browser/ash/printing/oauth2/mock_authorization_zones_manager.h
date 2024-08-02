// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_OAUTH2_MOCK_AUTHORIZATION_ZONES_MANAGER_H_
#define CHROME_BROWSER_ASH_PRINTING_OAUTH2_MOCK_AUTHORIZATION_ZONES_MANAGER_H_

#include <string>

#include "chrome/browser/ash/printing/oauth2/authorization_zones_manager.h"
#include "chrome/browser/ash/printing/oauth2/status_code.h"
#include "testing/gmock/include/gmock/gmock.h"

class GURL;

namespace chromeos {
class Uri;
}  // namespace chromeos

namespace syncer {
class DataTypeSyncBridge;
}  // namespace syncer

namespace ash::printing::oauth2 {

class MockAuthorizationZoneManager : public AuthorizationZonesManager {
 public:
  MockAuthorizationZoneManager();
  ~MockAuthorizationZoneManager() override;
  MOCK_METHOD(syncer::DataTypeSyncBridge*,
              GetDataTypeSyncBridge,
              (),
              (override));
  MOCK_METHOD(StatusCode,
              SaveAuthorizationServerAsTrusted,
              (const GURL& auth_server),
              (override));
  MOCK_METHOD(void,
              InitAuthorization,
              (const GURL& auth_server,
               const std::string& scope,
               StatusCallback callback),
              (override));
  MOCK_METHOD(void,
              FinishAuthorization,
              (const GURL& auth_server,
               const GURL& redirect_url,
               StatusCallback callback),
              (override));
  MOCK_METHOD(void,
              GetEndpointAccessToken,
              (const GURL& auth_server,
               const chromeos::Uri& ipp_endpoint,
               const std::string& scope,
               StatusCallback callback),
              (override));
  MOCK_METHOD(void,
              MarkEndpointAccessTokenAsExpired,
              (const GURL& auth_server,
               const chromeos::Uri& ipp_endpoint,
               const std::string& endpoint_access_token),
              (override));
};

}  // namespace ash::printing::oauth2

#endif  // CHROME_BROWSER_ASH_PRINTING_OAUTH2_MOCK_AUTHORIZATION_ZONES_MANAGER_H_
