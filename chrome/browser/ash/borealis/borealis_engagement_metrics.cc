// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_engagement_metrics.h"

#include "base/logging.h"
#include "chrome/browser/ash/borealis/borealis_prefs.h"
#include "chrome/browser/ash/borealis/borealis_service_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/borealis/borealis_util.h"
#include "components/exo/shell_surface_util.h"
#include "components/guest_os/guest_os_engagement_metrics.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace borealis {

constexpr char kUmaPrefix[] = "Borealis";

BorealisEngagementMetrics::BorealisEngagementMetrics(Profile* profile) {
  borealis_metrics_ = std::make_unique<guest_os::GuestOsEngagementMetrics>(
      profile->GetPrefs(),
      base::BindRepeating(&ash::borealis::IsBorealisWindow),
      prefs::kEngagementPrefsPrefix, kUmaPrefix);
  borealis_metrics_->SetBackgroundActive(true);
}

BorealisEngagementMetrics::~BorealisEngagementMetrics() {
  borealis_metrics_->SetBackgroundActive(false);
}

}  // namespace borealis
