// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scanner/chrome_scanner_delegate.h"

#include <utility>

#include "ash/public/cpp/scanner/scanner_feedback_info.h"
#include "ash/public/cpp/scanner/scanner_profile_scoped_delegate.h"
#include "ash/scanner/scanner_controller.h"
#include "chrome/browser/ash/scanner/scanner_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/scanner_feedback_dialog/scanner_feedback_dialog.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager.h"

ChromeScannerDelegate::ChromeScannerDelegate() = default;
ChromeScannerDelegate::~ChromeScannerDelegate() = default;

ash::ScannerProfileScopedDelegate*
ChromeScannerDelegate::GetProfileScopedDelegate() {
  // TODO(crbug.com/278643115): Take the account_id from the callers.
  const auto* active_session =
      session_manager::SessionManager::Get()->GetActiveSession();
  if (!active_session) {
    return nullptr;
  }
  return ScannerKeyedServiceFactory::GetForProfile(Profile::FromBrowserContext(
      ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
          active_session->account_id())));
}

void ChromeScannerDelegate::OpenFeedbackDialog(
    const AccountId& account_id,
    ash::ScannerFeedbackInfo feedback_info,
    SendFeedbackCallback send_feedback_callback) {
  content::BrowserContext* browser_context =
      ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
          account_id);

  auto* dialog = new ash::ScannerFeedbackDialog(
      std::move(feedback_info), std::move(send_feedback_callback));
  dialog->ShowSystemDialogForBrowserContext(browser_context);
}
