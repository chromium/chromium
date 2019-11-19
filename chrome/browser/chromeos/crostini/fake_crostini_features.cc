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

bool FakeCrostiniFeatures::IsAllowed(Profile* profile) {
  if (allowed_set_)
    return allowed_;
  return original_features_->IsAllowed(profile);
}

bool FakeCrostiniFeatures::IsUIAllowed(Profile* profile, bool check_policy) {
  if (ui_allowed_set_)
    return ui_allowed_;
  return original_features_->IsUIAllowed(profile, check_policy);
}

bool FakeCrostiniFeatures::IsEnabled(Profile* profile) {
  if (enabled_set_)
    return enabled_;
  return original_features_->IsEnabled(profile);
}

bool FakeCrostiniFeatures::IsExportImportUIAllowed(Profile* profile) {
  if (export_import_ui_allowed_set_)
    return export_import_ui_allowed_;
  return original_features_->IsExportImportUIAllowed(profile);
}

bool FakeCrostiniFeatures::IsRootAccessAllowed(Profile* profile) {
  if (root_access_allowed_set_)
    return root_access_allowed_;
  return original_features_->IsRootAccessAllowed(profile);
}

}  // namespace crostini
