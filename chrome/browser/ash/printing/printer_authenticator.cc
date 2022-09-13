// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/printer_authenticator.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "chrome/browser/ash/printing/cups_printers_manager.h"
#include "chrome/browser/ash/printing/oauth2/authorization_zones_manager.h"
#include "chrome/browser/ash/printing/oauth2/signin_dialog.h"
#include "chrome/browser/ash/printing/oauth2/status_code.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/printing/cups_printer_status.h"
#include "chromeos/printing/printer_configuration.h"
#include "chromeos/printing/uri.h"
#include "ui/views/window/dialog_delegate.h"
#include "url/gurl.h"

namespace ash::printing {

namespace {

// Shows to the user a dialog asking if given `auth_url` is a trusted
// Authorization Server.
void ShowIsTrustedDialog(const GURL& auth_url,
                         oauth2::StatusCallback callback) {
  // TODO(https://crbug.com/1223535): Add dialog asking the user if
  // the server is trusted. For now, we just save the server as trusted.
  std::move(callback).Run(oauth2::StatusCode::kOK, "");
}

// Shows to the user a dialog with webpage provided by the Authorization Server
// at `auth_url` and calls `callback` when the authorization procedure is
// completed or the dialog is closed by the user.
void ShowSigninDialog(const std::string& auth_url,
                      oauth2::StatusCallback callback) {
  const GURL url(auth_url);
  if (!url.is_valid()) {
    std::move(callback).Run(oauth2::StatusCode::kInvalidURL,
                            "auth_url=" + url.possibly_invalid_spec());
    return;
  }
  auto dialog = std::make_unique<oauth2::SigninDialog>(
      ProfileManager::GetPrimaryUserProfile());
  oauth2::SigninDialog* dialog_ptr = dialog.get();
  views::DialogDelegate::CreateDialogWidget(
      std::move(dialog), /*context=*/nullptr, /*parent=*/nullptr);
  dialog_ptr->StartAuthorizationProcedure(url, std::move(callback));
}

}  // namespace

PrinterAuthenticator::PrinterAuthenticator(
    CupsPrintersManager* cups_manager,
    oauth2::AuthorizationZonesManager* auth_manager,
    const chromeos::Printer& printer)
    : cups_manager_(cups_manager),
      auth_manager_(auth_manager),
      printer_(printer) {
  DCHECK(cups_manager);
  DCHECK(auth_manager);
}

PrinterAuthenticator::~PrinterAuthenticator() = default;

void PrinterAuthenticator::ObtainAccessTokenIfNeeded(
    oauth2::StatusCallback callback) {
  DCHECK(!callback_);
  callback_ = std::move(callback);
  cups_manager_->FetchPrinterStatus(
      printer_.id(), base::BindOnce(&PrinterAuthenticator::OnGetPrinterStatus,
                                    weak_factory_.GetWeakPtr()));
}

void PrinterAuthenticator::OnGetPrinterStatus(
    const chromeos::CupsPrinterStatus& printer_status) {
  const chromeos::PrinterAuthenticationInfo& auth_mode =
      printer_status.GetAuthenticationInfo();
  if (auth_mode.oauth_server.empty()) {
    // A printer does not require authentication.
    std::move(callback_).Run(oauth2::StatusCode::kOK, "");
    return;
  }

  oauth_server_ = GURL(auth_mode.oauth_server);
  if (!oauth_server_.is_valid()) {
    std::move(callback_).Run(oauth2::StatusCode::kInvalidURL, "");
    return;
  }
  oauth_scope_ = auth_mode.oauth_scope;

  auth_manager_->GetEndpointAccessToken(oauth_server_, printer_.uri(),
                                        oauth_scope_,
                                        OnComplete(Step::kGetAccessToken));
}

oauth2::StatusCallback PrinterAuthenticator::OnComplete(Step step) {
  return base::BindOnce(&PrinterAuthenticator::ToNextStep,
                        weak_factory_.GetWeakPtr(), step);
}

void PrinterAuthenticator::ToNextStep(PrinterAuthenticator::Step current_step,
                                      oauth2::StatusCode status,
                                      const std::string& data) {
  switch (current_step) {
    case Step::kGetAccessToken:
      if (status == oauth2::StatusCode::kOK) {
        // Success, return the endpoint access token.
        std::move(callback_).Run(status, data);
        return;
      }
      if (status == oauth2::StatusCode::kUntrustedAuthorizationServer) {
        ShowIsTrustedDialog(oauth_server_,
                            OnComplete(Step::kShowIsTrustedDialog));
        return;
      }
      if (status == oauth2::StatusCode::kAuthorizationNeeded) {
        auth_manager_->InitAuthorization(oauth_server_, oauth_scope_,
                                         OnComplete(Step::kInitAuthorization));
        return;
      }
      break;
    case Step::kShowIsTrustedDialog:
      if (status == oauth2::StatusCode::kOK) {
        status = auth_manager_->SaveAuthorizationServerAsTrusted(oauth_server_);
        if (status == oauth2::StatusCode::kOK) {
          auth_manager_->InitAuthorization(
              oauth_server_, oauth_scope_,
              OnComplete(Step::kInitAuthorization));
          return;
        }
      }
      break;
    case Step::kInitAuthorization:
      if (status == oauth2::StatusCode::kOK) {
        ShowSigninDialog(data, OnComplete(Step::kShowSigninDialog));
        return;
      }
      if (status == oauth2::StatusCode::kUntrustedAuthorizationServer) {
        ShowIsTrustedDialog(oauth_server_,
                            OnComplete(Step::kShowIsTrustedDialog));
        return;
      }
      break;
    case Step::kShowSigninDialog:
      if (status == oauth2::StatusCode::kOK) {
        auth_manager_->FinishAuthorization(
            oauth_server_, GURL(data), OnComplete(Step::kFinishAuthorization));
        return;
      }
      break;
    case Step::kFinishAuthorization:
      if (status == oauth2::StatusCode::kOK) {
        auth_manager_->GetEndpointAccessToken(
            oauth_server_, printer_.uri(), oauth_scope_,
            OnComplete(Step::kGetAccessToken));
        return;
      }
      break;
  }

  // An error occurred.
  std::move(callback_).Run(status, "");
}

}  // namespace ash::printing
