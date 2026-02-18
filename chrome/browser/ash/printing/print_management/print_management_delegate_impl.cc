// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/print_management/print_management_delegate_impl.h"

#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "chromeos/ash/experiences/settings_ui/settings_app_manager.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_manager.h"

namespace ash::print_management {

void PrintManagementDelegateImpl::LaunchPrinterSettings() {
  auto* session = session_manager::SessionManager::Get()->GetPrimarySession();
  CHECK(session);
  auto* user =
      user_manager::UserManager::Get()->FindUser(session->account_id());
  ash::SettingsAppManager::Get()->Open(
      CHECK_DEREF(user),
      {.sub_page = chromeos::settings::mojom::kPrintingDetailsSubpagePath});
}

}  // namespace ash::print_management
