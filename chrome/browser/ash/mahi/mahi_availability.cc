// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/mahi/mahi_availability.h"

#include "ash/constants/generative_ai_country_restrictions.h"
#include "base/command_line.h"
#include "base/containers/fixed_flat_set.h"
#include "chrome/browser/browser_process.h"
#include "chromeos/ash/components/demo_mode/utils/demo_session_utils.h"
#include "chromeos/ash/components/manta/manta_service_provider.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/account_id/account_id.h"
#include "components/manta/manta_service.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_names.h"
#include "components/variations/service/variations_service.h"

namespace ash::mahi_availability {

base::expected<bool, Error> CanUseMahiService() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kMahiRestrictionsOverride)) {
    return true;
  }

  if (!ash::demo_mode::IsDeviceInDemoMode()) {
    // TODO(crbug.com/278643115): Take the account_id from the callers.
    const auto* session =
        session_manager::SessionManager::Get()->GetActiveSession();
    if (!session) {
      return false;
    }

    // Guest session is not allowed when not in demo mode.
    if (session->account_id() == user_manager::GuestAccountId()) {
      return false;
    }

    // The service may still be null when manta is disabled for the user.
    if (manta::MantaService* service =
            MantaServiceProvider::Get().Find(session->account_id());
        service) {
      switch (service->CanAccessMantaFeaturesWithoutMinorRestrictions()) {
        case manta::FeatureSupportStatus::kSupported:
          break;
        case manta::FeatureSupportStatus::kUnsupported:
          return false;
        case manta::FeatureSupportStatus::kUnknown:
          return base::unexpected(Error::kMantaFeatureBitNotReady);
      }
    }
  }

  const std::string country_code =
      (g_browser_process != nullptr &&
       g_browser_process->variations_service() != nullptr)
          ? g_browser_process->variations_service()->GetLatestCountry()
          : std::string();

  return IsGenerativeAiAllowedForCountry(country_code);
}

base::expected<bool, Error> IsMahiAvailable() {
  if (!chromeos::features::IsMahiEnabled()) {
    return false;
  }

  return CanUseMahiService();
}

}  // namespace ash::mahi_availability
