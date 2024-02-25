// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_PRINTER_AUTHENTICATOR_H_
#define CHROME_BROWSER_ASH_PRINTING_PRINTER_AUTHENTICATOR_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/printing/oauth2/status_code.h"
#include "chromeos/printing/cups_printer_status.h"
#include "chromeos/printing/printer_configuration.h"
#include "chromeos/printing/uri.h"
#include "url/gurl.h"

namespace ash {
class CupsPrintersManager;
}  // namespace ash

namespace chromeos {
class Printer;
}  // namespace chromeos

namespace ash::printing {

namespace oauth2 {
class AuthorizationZonesManager;
}  // namespace oauth2

class PrinterAuthenticator {
 public:
  // No nullptrs are allowed (DCHECK).
  PrinterAuthenticator(CupsPrintersManager* printers_manager,
                       oauth2::AuthorizationZonesManager* auth_manager,
                       const chromeos::Printer& printer);

  PrinterAuthenticator(const PrinterAuthenticator&) = delete;
  PrinterAuthenticator& operator=(const PrinterAuthenticator&) = delete;

  ~PrinterAuthenticator();

  // Starts the procedure of obtainig access to the printer. It may involve
  // showing dialogs to the user and waiting for his reaction. `callback` is
  // called when the procedure is completed. Three types of responses (sent as
  // callback's parameters) are possible:
  //  * `status` == StatusCode::kOK && `data` == "": access granted, no access
  //    tokens are needed;
  //  * `status` == StatusCode::kOK && `data` != "": access granted, `data` must
  //    be used as an access token during communication with the printer;
  //  * `status` != StatusCode::kOK: access denied or an error occurred, use the
  //    `status` to choose an error message shown to the user.
  void ObtainAccessTokenIfNeeded(oauth2::StatusCallback callback);

  // Instead of showing UI dialogs to user, statuses from
  // `is_trusted_dialog_response` and `signin_dialog_response` are
  // returned.
  void SetUIResponsesForTesting(oauth2::StatusCode is_trusted_dialog_response,
                                oauth2::StatusCode signin_dialog_response);

 private:
  // Enumerates steps of an authorization procedure.
  enum class Step {
    kGetAccessToken,
    kShowIsTrustedDialog,
    kInitAuthorization,
    kShowSigninDialog,
    kFinishAuthorization
  };

  // This is called when the status of the printer is obtained.
  void OnGetPrinterStatus(const chromeos::CupsPrinterStatus& printer_status);
  // Returns a callback that must be called when the given `step` is completed.
  oauth2::StatusCallback OnComplete(Step step);
  // Proceeds to the next action when `current_step` is completed. `status` and
  // `data` contain result of the completed step (`current_step`).
  void ToNextStep(Step current_step,
                  oauth2::StatusCode status,
                  std::string data);
  // Shows a dialog to the user asking if the given `auth_url` is a trusted
  // Authorization Server. Calls `callback` when the dialog is closed.
  void ShowIsTrustedDialog(const GURL& auth_url,
                           oauth2::StatusCallback callback);

  // Shows a dialog to the user with the webpage provided by the Authorization
  // Server at `auth_url` and calls `callback` when the authorization procedure
  // is completed or when the dialog is closed by the user.
  void ShowSigninDialog(const std::string& auth_url,
                        oauth2::StatusCallback callback);

  raw_ptr<CupsPrintersManager> cups_manager_;
  raw_ptr<oauth2::AuthorizationZonesManager> auth_manager_;
  chromeos::Printer printer_;
  GURL oauth_server_;
  std::string oauth_scope_;
  oauth2::StatusCallback callback_;

  // These fields are used to avoid showing UI elements when the class is
  // tested in unit tests.
  std::optional<oauth2::StatusCode> is_trusted_dialog_response_for_testing_;
  std::optional<oauth2::StatusCode> signin_dialog_response_for_testing_;

  base::WeakPtrFactory<PrinterAuthenticator> weak_factory_{this};
};

}  // namespace ash::printing

#endif  // CHROME_BROWSER_ASH_PRINTING_PRINTER_AUTHENTICATOR_H_
