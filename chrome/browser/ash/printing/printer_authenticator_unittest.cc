// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/printer_authenticator.h"

#include <memory>
#include <string>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ash/printing/fake_cups_printers_manager.h"
#include "chrome/browser/ash/printing/oauth2/mock_authorization_zones_manager.h"
#include "chrome/browser/ash/printing/oauth2/status_code.h"
#include "chromeos/printing/cups_printer_status.h"
#include "chromeos/printing/printer_configuration.h"
#include "chromeos/printing/uri.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash::printing {
namespace {

// Represents results returned by callback void(StatusCode, const string&).
struct CallbackResult {
  oauth2::StatusCode status = oauth2::StatusCode::kUnexpectedError;
  std::string data;
};

class PrintingPrinterAuthenticatorTest : public testing::Test {
 public:
  PrintingPrinterAuthenticatorTest() : printer_(kPrinterId) {}

 protected:
  // Init PrintAuthenticator object being tested. Values of parameters define
  // results returned by simulated UI dialogs.
  void CreateAuthenticator(oauth2::StatusCode is_trusted_dialog_response =
                               oauth2::StatusCode::kUnexpectedError,
                           oauth2::StatusCode signin_dialog_response =
                               oauth2::StatusCode::kUnexpectedError) {
    authenticator_ = std::make_unique<PrinterAuthenticator>(
        &cups_manager_, &auth_manager_, printer_);
    authenticator_->SetUIResponsesForTesting(is_trusted_dialog_response,
                                             signin_dialog_response);
  }

  // Call PrintAuthenticator::ObtainAccessTokenIfNeeded() and returns result
  // reported by the callback.
  CallbackResult CallObtainAccessTokenIfNeeded() {
    base::MockOnceCallback<void(oauth2::StatusCode, std::string)> callback;
    CallbackResult result;
    base::RunLoop loop;
    EXPECT_CALL(callback, Run)
        .InSequence(sequence_)
        .WillOnce(
            [&result, &loop](oauth2::StatusCode status, std::string data) {
              result.status = status;
              result.data = std::move(data);
              loop.Quit();
            });
    authenticator_->ObtainAccessTokenIfNeeded(callback.Get());
    loop.Run();
    return result;
  }

  // Mock CupsPrintersManager::FetchPrinterStatus. `auth_mode` is passed to the
  // object chromeos::CupsPrinterStatus returned in the callback.
  void ExpectCallFetchPrinterStatus(
      const chromeos::PrinterAuthenticationInfo& auth_mode) {
    chromeos::CupsPrinterStatus printer_status(kPrinterId);
    printer_status.SetAuthenticationInfo(auth_mode);
    cups_manager_.SetPrinterStatus(printer_status);
  }

  // Mock AuthorizationZoneManager::SaveAuthorizationServerAsTrusted. `result`
  // will be return by the call.
  void ExpectCallSaveAuthorizationServerAsTrusted(oauth2::StatusCode result) {
    EXPECT_CALL(auth_manager_, SaveAuthorizationServerAsTrusted(testing::_))
        .InSequence(sequence_)
        .WillOnce([result](const GURL& auth_server) { return result; });
  }

  // Mock AuthorizationZoneManager::InitAuthorization. Callback passed to the
  // mocked method will be called with parameters set in `result`.
  void ExpectCallInitAuthorization(CallbackResult result) {
    EXPECT_CALL(auth_manager_,
                InitAuthorization(testing::_, testing::_, testing::_))
        .InSequence(sequence_)
        .WillOnce([result](const GURL& auth_server, const std::string& scope,
                           oauth2::StatusCallback callback) {
          std::move(callback).Run(result.status, std::move(result.data));
        });
  }

  // Mock AuthorizationZoneManager::FinishAuthorization. Callback passed to the
  // mocked method will be called with parameters set in `result`.
  void ExpectCallFinishAuthorization(CallbackResult result) {
    EXPECT_CALL(auth_manager_,
                FinishAuthorization(testing::_, testing::_, testing::_))
        .InSequence(sequence_)
        .WillOnce([result](const GURL& auth_server, const GURL& redirect_url,
                           oauth2::StatusCallback callback) {
          std::move(callback).Run(result.status, std::move(result.data));
        });
  }

  // Mock AuthorizationZoneManager::GetEndpointAccessToken. Callback passed to
  // the mocked method will be called with parameters set in `result`.
  void ExpectCallGetEndpointAccessToken(CallbackResult result) {
    EXPECT_CALL(auth_manager_, GetEndpointAccessToken(testing::_, testing::_,
                                                      testing::_, testing::_))
        .InSequence(sequence_)
        .WillOnce([result](const GURL& auth_server,
                           const chromeos::Uri& ipp_endpoint,
                           const std::string& scope,
                           oauth2::StatusCallback callback) {
          std::move(callback).Run(result.status, std::move(result.data));
        });
  }

  const std::string kPrinterId = "printer_id";
  const chromeos::PrinterAuthenticationInfo kAuthMode = {"https://auth/server",
                                                         "scope1 scope2"};
  const std::string kAuthURL = "https://auth/server/login?ala=ma;kota";

 private:
  content::BrowserTaskEnvironment task_environment_;
  testing::Sequence sequence_;
  FakeCupsPrintersManager cups_manager_;
  testing::StrictMock<oauth2::MockAuthorizationZoneManager> auth_manager_;
  chromeos::Printer printer_;
  std::unique_ptr<PrinterAuthenticator> authenticator_;
};

TEST_F(PrintingPrinterAuthenticatorTest, AuthenticationNotNeeded) {
  CreateAuthenticator();
  ExpectCallFetchPrinterStatus({"", ""});
  CallbackResult result = CallObtainAccessTokenIfNeeded();
  EXPECT_EQ(result.status, oauth2::StatusCode::kOK);
  EXPECT_TRUE(result.data.empty());
}

TEST_F(PrintingPrinterAuthenticatorTest, AlreadyAuthenticated) {
  CreateAuthenticator(oauth2::StatusCode::kOK);
  ExpectCallFetchPrinterStatus(kAuthMode);
  ExpectCallGetEndpointAccessToken({oauth2::StatusCode::kOK, "access token"});
  CallbackResult result = CallObtainAccessTokenIfNeeded();
  EXPECT_EQ(result.status, oauth2::StatusCode::kOK);
  EXPECT_EQ(result.data, "access token");
}

TEST_F(PrintingPrinterAuthenticatorTest, ServerNotTrusted) {
  CreateAuthenticator(oauth2::StatusCode::kUntrustedAuthorizationServer);
  ExpectCallFetchPrinterStatus(kAuthMode);
  ExpectCallGetEndpointAccessToken(
      {oauth2::StatusCode::kUntrustedAuthorizationServer});
  // Response from IsTrustedDialog: StatusCode::kUntrustedAuthorizationServer
  CallbackResult result = CallObtainAccessTokenIfNeeded();
  EXPECT_EQ(result.status, oauth2::StatusCode::kUntrustedAuthorizationServer);
  EXPECT_TRUE(result.data.empty());
}

TEST_F(PrintingPrinterAuthenticatorTest, ServerSavedAsTrusted) {
  CreateAuthenticator(oauth2::StatusCode::kOK);
  ExpectCallFetchPrinterStatus(kAuthMode);
  ExpectCallGetEndpointAccessToken(
      {oauth2::StatusCode::kUntrustedAuthorizationServer});
  // Response from IsTrustedDialog: StatusCode::kOK
  ExpectCallSaveAuthorizationServerAsTrusted(oauth2::StatusCode::kOK);
  ExpectCallGetEndpointAccessToken(
      {oauth2::StatusCode::kClientNotRegistered, "error"});
  CallbackResult result = CallObtainAccessTokenIfNeeded();
  EXPECT_EQ(result.status, oauth2::StatusCode::kClientNotRegistered);
  EXPECT_TRUE(result.data.empty());
}

TEST_F(PrintingPrinterAuthenticatorTest, InvalidAuthorizationURL) {
  CreateAuthenticator();
  ExpectCallFetchPrinterStatus(kAuthMode);
  ExpectCallGetEndpointAccessToken({oauth2::StatusCode::kAuthorizationNeeded});
  ExpectCallInitAuthorization({oauth2::StatusCode::kOK, "invalid_auth_url"});
  // Response from SigninDialog: StatusCode::kInvalidURL
  CallbackResult result = CallObtainAccessTokenIfNeeded();
  EXPECT_EQ(result.status, oauth2::StatusCode::kInvalidURL);
  EXPECT_TRUE(result.data.empty());
}

TEST_F(PrintingPrinterAuthenticatorTest, AuthorizationError) {
  CreateAuthenticator(oauth2::StatusCode::kOK,
                      oauth2::StatusCode::kServerError);
  ExpectCallFetchPrinterStatus(kAuthMode);
  ExpectCallGetEndpointAccessToken({oauth2::StatusCode::kAuthorizationNeeded});
  ExpectCallInitAuthorization({oauth2::StatusCode::kOK, kAuthURL});
  // Response from SigninDialog: StatusCode::kServerError
  CallbackResult result = CallObtainAccessTokenIfNeeded();
  EXPECT_EQ(result.status, oauth2::StatusCode::kServerError);
  EXPECT_TRUE(result.data.empty());
}

TEST_F(PrintingPrinterAuthenticatorTest, AccessDenied) {
  CreateAuthenticator(oauth2::StatusCode::kOK, oauth2::StatusCode::kOK);
  ExpectCallFetchPrinterStatus(kAuthMode);
  ExpectCallGetEndpointAccessToken({oauth2::StatusCode::kAuthorizationNeeded});
  ExpectCallInitAuthorization({oauth2::StatusCode::kOK, kAuthURL});
  // Response from SigninDialog: StatusCode::kOK
  ExpectCallFinishAuthorization({oauth2::StatusCode::kAccessDenied, "error"});
  CallbackResult result = CallObtainAccessTokenIfNeeded();
  EXPECT_EQ(result.status, oauth2::StatusCode::kAccessDenied);
  EXPECT_TRUE(result.data.empty());
}

TEST_F(PrintingPrinterAuthenticatorTest, AuthorizationSuccessful) {
  CreateAuthenticator(oauth2::StatusCode::kOK, oauth2::StatusCode::kOK);
  ExpectCallFetchPrinterStatus(kAuthMode);
  ExpectCallGetEndpointAccessToken({oauth2::StatusCode::kAuthorizationNeeded});
  ExpectCallInitAuthorization({oauth2::StatusCode::kOK, kAuthURL});
  // Response from SigninDialog: StatusCode::kOK
  ExpectCallFinishAuthorization({oauth2::StatusCode::kOK});
  ExpectCallGetEndpointAccessToken({oauth2::StatusCode::kOK, "access token"});
  CallbackResult result = CallObtainAccessTokenIfNeeded();
  EXPECT_EQ(result.status, oauth2::StatusCode::kOK);
  EXPECT_EQ(result.data, "access token");
}

}  // namespace
}  // namespace ash::printing
