// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/fake_crostini_features.h"

namespace crostini {

FakeCrostiniFeatures::FakeCrostiniFeatures() {
  original_features_ = CrostiniFeatures::Get();
  CrostiniFeatures::SetForTesting(this);
}

FakeCrostiniFeatures::~FakeCrostiniFeatures() {
  CrostiniFeatures::SetForTesting(original_features_);
}

void FakeCrostiniFeatures::SetAll(bool flag) {
  could_be_allowed_ = flag;
  allowed_now_ = flag;
  enabled_ = flag;
  export_import_ui_allowed_ = flag;
  root_access_allowed_ = flag;
  container_upgrade_ui_allowed_ = flag;
  can_change_adb_sideloading_ = flag;
  port_forwarding_allowed_ = flag;
}

void FakeCrostiniFeatures::ClearAll() {
  could_be_allowed_ = base::nullopt;
  allowed_now_ = base::nullopt;
  enabled_ = base::nullopt;
  export_import_ui_allowed_ = base::nullopt;
  root_access_allowed_ = base::nullopt;
  container_upgrade_ui_allowed_ = base::nullopt;
  can_change_adb_sideloading_ = base::nullopt;
  port_forwarding_allowed_ = base::nullopt;
}

bool FakeCrostiniFeatures::CouldBeAllowed(Profile* profile,
                                          std::string* reason) {
  if (could_be_allowed_.has_value()) {
    *reason = "some reason";
    return *could_be_allowed_;
  }
  return original_features_->CouldBeAllowed(profile, reason);
}

bool FakeCrostiniFeatures::IsAllowedNow(Profile* profile, std::string* reason) {
  if (could_be_allowed_.has_value() && !could_be_allowed_) {
    *reason = "some reason";
    return false;
  }
  if (allowed_now_.has_value()) {
    *reason = "some reason";
    return *allowed_now_;
  }
  return original_features_->IsAllowedNow(profile, reason);
}

bool FakeCrostiniFeatures::IsEnabled(Profile* profile) {
  if (enabled_.has_value())
    return *enabled_;
  return original_features_->IsEnabled(profile);
}

bool FakeCrostiniFeatures::IsExportImportUIAllowed(Profile* profile) {
  if (export_import_ui_allowed_.has_value())
    return *export_import_ui_allowed_;
  return original_features_->IsExportImportUIAllowed(profile);
}

bool FakeCrostiniFeatures::IsRootAccessAllowed(Profile* profile) {
  if (root_access_allowed_.has_value())
    return *root_access_allowed_;
  return original_features_->IsRootAccessAllowed(profile);
}

bool FakeCrostiniFeatures::IsContainerUpgradeUIAllowed(Profile* profile) {
  if (container_upgrade_ui_allowed_.has_value())
    return *container_upgrade_ui_allowed_;
  return original_features_->IsContainerUpgradeUIAllowed(profile);
}

void FakeCrostiniFeatures::CanChangeAdbSideloading(
    Profile* profile,
    CanChangeAdbSideloadingCallback callback) {
  if (can_change_adb_sideloading_.has_value()) {
    std::move(callback).Run(*can_change_adb_sideloading_);
    return;
  }
  original_features_->CanChangeAdbSideloading(profile, std::move(callback));
}

bool FakeCrostiniFeatures::IsPortForwardingAllowed(Profile* profile) {
  if (port_forwarding_allowed_.has_value())
    return *port_forwarding_allowed_;
  return original_features_->IsPortForwardingAllowed(profile);
}

}  // namespace crostini
