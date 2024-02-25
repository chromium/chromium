// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/printer_authenticator.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/printing/cups_printers_manager.h"
#include "chrome/browser/ash/printing/oauth2/authorization_zones_manager.h"
#include "chrome/browser/ash/printing/oauth2/log_entry.h"
#include "chrome/browser/ash/printing/oauth2/signin_dialog.h"
#include "chrome/browser/ash/printing/oauth2/status_code.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/printing/cups_printer_status.h"
#include "chromeos/printing/printer_configuration.h"
#include "chromeos/printing/uri.h"
#include "components/device_event_log/device_event_log.h"
#include "ui/views/window/dialog_delegate.h"
#include "url/gurl.h"

namespace ash::printing {

namespace {

// Logs results to device-log and calls `callback` with parameters `status` and
// `data`.
void LogAndCall(oauth2::StatusCallback callback,
                std::string_view method,
                const GURL& auth_server,
                oauth2::StatusCode status,
                std::string data) {
  if (status == oauth2::StatusCode::kOK) {
    PRINTER_LOG(EVENT) << oauth2::LogEntry("", method, auth_server, status);
  } else {
    PRINTER_LOG(ERROR) << oauth2::LogEntry(data, method, auth_server, status);
  }
  std::move(callback).Run(status, std::move(data));
}

}  // namespace

PrinterAuthenticator::PrinterAuthenticator(
    CupsPrintersManager* printers_manager,
    oauth2::AuthorizationZonesManager* auth_manager,
    const chromeos::Printer& printer)
    : cups_manager_(printers_manager),
      auth_manager_(auth_manager),
      printer_(printer) {
  DCHECK(printers_manager);
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

void PrinterAuthenticator::SetUIResponsesForTesting(
    oauth2::StatusCode is_trusted_dialog_response,
    oauth2::StatusCode signin_dialog_response) {
  is_trusted_dialog_response_for_testing_ = is_trusted_dialog_response;
  signin_dialog_response_for_testing_ = signin_dialog_response;
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
                                      std::string data) {
  switch (current_step) {
    case Step::kGetAccessToken:
      if (status == oauth2::StatusCode::kOK) {
        // Success, return the endpoint access token.
        std::move(callback_).Run(status, std::move(data));
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
          auth_manager_->GetEndpointAccessToken(
              oauth_server_, printer_.uri(), oauth_scope_,
              OnComplete(Step::kGetAccessToken));
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

void PrinterAuthenticator::ShowIsTrustedDialog(
    const GURL& auth_url,
    oauth2::StatusCallback callback) {
  // Log the callback to device-log.
  callback =
      base::BindOnce(&LogAndCall, std::move(callback), __func__, oauth_server_);

  if (is_trusted_dialog_response_for_testing_) {
    std::move(callback).Run(*is_trusted_dialog_response_for_testing_,
                            "response from mock");
    return;
  }
  // TODO(https://crbug.com/1223535): Add dialog asking the user if
  // the server is trusted. For now, we just save the server as trusted.
  std::move(callback).Run(oauth2::StatusCode::kOK, "");
}

void PrinterAuthenticator::ShowSigninDialog(const std::string& auth_url,
                                            oauth2::StatusCallback callback) {
  // Log the callback to device-log.
  callback =
      base::BindOnce(&LogAndCall, std::move(callback), __func__, oauth_server_);

  const GURL url(auth_url);
  if (!url.is_valid()) {
    std::move(callback).Run(oauth2::StatusCode::kInvalidURL,
                            "auth_url=" + url.possibly_invalid_spec());
    return;
  }

  if (signin_dialog_response_for_testing_) {
    std::move(callback).Run(*signin_dialog_response_for_testing_,
                            "response from mock");
    return;
  }

  auto dialog = std::make_unique<oauth2::SigninDialog>(
      ProfileManager::GetPrimaryUserProfile());
  oauth2::SigninDialog* dialog_ptr = dialog.get();
  views::DialogDelegate::CreateDialogWidget(
      std::move(dialog), /*context=*/nullptr, /*parent=*/nullptr);
  dialog_ptr->StartAuthorizationProcedure(url, std::move(callback));
}

}  // namespace ash::printing
