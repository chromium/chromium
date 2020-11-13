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
  allowed_ = flag;
  ui_allowed_ = flag;
  policy_allowed_ = flag;
  enabled_ = flag;
  export_import_ui_allowed_ = flag;
  root_access_allowed_ = flag;
  container_upgrade_ui_allowed_ = flag;
  can_change_adb_sideloading_ = flag;
  port_forwarding_allowed_ = flag;
}

void FakeCrostiniFeatures::ClearAll() {
  allowed_ = base::nullopt;
  ui_allowed_ = base::nullopt;
  policy_allowed_ = base::nullopt;
  enabled_ = base::nullopt;
  export_import_ui_allowed_ = base::nullopt;
  root_access_allowed_ = base::nullopt;
  container_upgrade_ui_allowed_ = base::nullopt;
  can_change_adb_sideloading_ = base::nullopt;
  port_forwarding_allowed_ = base::nullopt;
}

bool FakeCrostiniFeatures::IsAllowed(Profile* profile) {
  if (allowed_.has_value())
    return *allowed_;
  return original_features_->IsAllowed(profile);
}

bool FakeCrostiniFeatures::IsUIAllowed(Profile* profile, bool check_policy) {
  if (check_policy && policy_allowed_.has_value() && !policy_allowed_)
    return false;
  if (ui_allowed_.has_value())
    return *ui_allowed_;
  return original_features_->IsUIAllowed(profile, check_policy);
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
