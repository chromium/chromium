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

  signin::IdentityManager* identity_manager();
  network::mojom::URLLoaderFactory* GetUrlLoaderFactory();

  // ProjectorAppClient:
  MOCK_METHOD(void, AddObserver, (Observer*), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer*), (override));
  MOCK_METHOD(bool, IsActiveAccount, (const AccountId&), (const, override));
  MOCK_METHOD(void,
              OnNewScreencastPreconditionChanged,
              (const NewScreencastPrecondition&),
              (override));
  MOCK_METHOD(const PendingScreencastContainerSet&,
              GetPendingScreencasts,
              (const AccountId&),
              (const, override));
  MOCK_METHOD(bool, ShouldDownloadSoda, (), (const, override));
  MOCK_METHOD(void, InstallSoda, (), (override));
  MOCK_METHOD(void, OnSodaInstallProgress, (int), (override));
  MOCK_METHOD(void, OnSodaInstallError, (), (override));
  MOCK_METHOD(void, OnSodaInstalled, (), (override));
  MOCK_METHOD(void, OpenFeedbackDialog, (const AccountId&), (const, override));
  MOCK_METHOD(void,
              GetVideo,
              (const AccountId&,
               const std::string&,
               const std::optional<std::string>&,
               ProjectorAppClient::OnGetVideoCallback),
              (const, override));
  MOCK_METHOD(void, NotifyAppUIActive, (const AccountId&, bool), (override));
  MOCK_METHOD(void,
              ToggleFileSyncingNotificationForPaths,
              (const std::vector<base::FilePath>&, bool),
              (override));
  MOCK_METHOD(void,
              HandleAccountReauth,
              (const AccountId&, const std::string&),
              (override));

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
