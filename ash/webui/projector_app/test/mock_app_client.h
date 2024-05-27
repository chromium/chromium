// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PROJECTOR_APP_TEST_MOCK_APP_CLIENT_H_
#define ASH_WEBUI_PROJECTOR_APP_TEST_MOCK_APP_CLIENT_H_

#include <string>

#include "ash/webui/projector_app/projector_app_client.h"
#include "base/time/time.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace network {
namespace mojom {
class URLLoaderFactory;
}  // namespace mojom
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace ash {

class MockAppClient : public ProjectorAppClient {
 public:
  MockAppClient();
  MockAppClient(const MockAppClient&) = delete;
  MockAppClient& operator=(const MockAppClient&) = delete;
  ~MockAppClient() override;

  network::TestURLLoaderFactory& test_url_loader_factory() {
    return test_url_loader_factory_;
  }

  // ProjectorAppClient:
  signin::IdentityManager* GetIdentityManager() override;
  network::mojom::URLLoaderFactory* GetUrlLoaderFactory() override;

  MOCK_METHOD1(AddObserver, void(Observer*));
  MOCK_METHOD1(RemoveObserver, void(Observer*));
  MOCK_METHOD1(OnNewScreencastPreconditionChanged,
               void(const NewScreencastPrecondition&));
  MOCK_CONST_METHOD0(GetPendingScreencasts,
                     const PendingScreencastContainerSet&());
  MOCK_CONST_METHOD0(ShouldDownloadSoda, bool());
  MOCK_METHOD0(InstallSoda, void());
  MOCK_METHOD1(OnSodaInstallProgress, void(int));
  MOCK_METHOD0(OnSodaInstallError, void());
  MOCK_METHOD0(OnSodaInstalled, void());
  MOCK_CONST_METHOD0(OpenFeedbackDialog, void());
  MOCK_CONST_METHOD3(GetVideo,
                     void(const std::string&,
                          const std::optional<std::string>&,
                          ProjectorAppClient::OnGetVideoCallback));
  MOCK_METHOD1(NotifyAppUIActive, void(bool active));
  MOCK_METHOD2(ToggleFileSyncingNotificationForPaths,
               void(const std::vector<base::FilePath>&, bool));
  MOCK_METHOD1(HandleAccountReauth, void(const std::string&));

  void SetAutomaticIssueOfAccessTokens(bool success);
  void WaitForAccessRequest(const std::string& account_email);
  void GrantOAuthTokenFor(const std::string& account_email,
                          const base::Time& expiry_time);
  void AddSecondaryAccount(const std::string& account_email);
  void MakeFetchTokenFailWithError(const GoogleServiceAuthError& error);

 private:
  signin::IdentityTestEnvironment identity_test_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
};

}  // namespace ash

#endif  // ASH_WEBUI_PROJECTOR_APP_TEST_MOCK_APP_CLIENT_H_
