// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/ash_dns_over_https_config_source.h"

#include <string>

#include "base/functional/callback_forward.h"
#include "chrome/browser/net/secure_dns_config.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace ash {

AshDnsOverHttpsConfigSource::AshDnsOverHttpsConfigSource(
    SecureDnsManager* secure_dns_manager,
    PrefService* local_state)
    : dns_over_https_mode_(SecureDnsConfig::kModeOff),
      local_state_(local_state),
      secure_dns_manager_(secure_dns_manager) {
  secure_dns_manager_->AddObserver(this);
}

AshDnsOverHttpsConfigSource::~AshDnsOverHttpsConfigSource() {
  if (secure_dns_manager_) {
    secure_dns_manager_->RemoveObserver(this);
  }
}

std::string AshDnsOverHttpsConfigSource::GetDnsOverHttpsMode() const {
  return secure_dns_manager_ ? dns_over_https_mode_ : std::string();
}

std::string AshDnsOverHttpsConfigSource::GetDnsOverHttpsTemplates() const {
  return secure_dns_manager_ ? dns_over_https_templates_ : std::string();
}

bool AshDnsOverHttpsConfigSource::IsConfigManaged() const {
  return local_state_->FindPreference(prefs::kDnsOverHttpsMode)->IsManaged();
}

void AshDnsOverHttpsConfigSource::SetDohChangeCallback(
    base::RepeatingClosure callback) {
  CHECK(!on_change_callback_)
      << "This instance of AshDnsOverHttpsConfigSource already has an observer";
  on_change_callback_ = callback;
}

void AshDnsOverHttpsConfigSource::OnTemplateUrisChanged(
    const std::string& template_uris) {
  dns_over_https_templates_ = template_uris;
  if (on_change_callback_) {
    on_change_callback_.Run();
  }
}

void AshDnsOverHttpsConfigSource::OnModeChanged(const std::string& mode) {
  dns_over_https_mode_ = mode;
  if (on_change_callback_) {
    on_change_callback_.Run();
  }
}

void AshDnsOverHttpsConfigSource::OnSecureDnsManagerShutdown() {
  secure_dns_manager_->RemoveObserver(this);
  secure_dns_manager_ = nullptr;
}

}  // namespace ash
