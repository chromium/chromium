// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/print_management/print_management_delegate_impl.h"

#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/check.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/user_manager/user_manager.h"

namespace ash::print_management {

namespace {

// Look up primary user's profile.
Profile* GetProfile() {
  return Profile::FromBrowserContext(
      ash::BrowserContextHelper::Get()->GetBrowserContextByUser(
          user_manager::UserManager::Get()->GetPrimaryUser()));
}

}  // namespace

void PrintManagementDelegateImpl::LaunchPrinterSettings() {
  auto* profile = GetProfile();
  CHECK(profile);
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      profile, chromeos::settings::mojom::kPrintingDetailsSubpagePath);
}

}  // namespace ash::print_management
