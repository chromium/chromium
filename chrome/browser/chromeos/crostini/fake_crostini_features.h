// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_FAKE_CROSTINI_FEATURES_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_FAKE_CROSTINI_FEATURES_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/crostini/crostini_features.h"

class Profile;

namespace crostini {

// FakeCrostiniFeatures implements a fake version of CrostiniFeatures which can
// be used for testing.  It captures the current global CrostiniFeatures object
// and replaces it for the scope of this object.  It overrides only the
// features that you set and uses the previous object for other features.
class FakeCrostiniFeatures : public CrostiniFeatures {
 public:
  FakeCrostiniFeatures();
  ~FakeCrostiniFeatures() override;

  // CrostiniFeatures:
  bool IsAllowed(Profile* profile) override;
  bool IsUIAllowed(Profile* profile, bool check_policy) override;
  bool IsEnabled(Profile* profile) override;
  bool IsExportImportUIAllowed(Profile* profile) override;
  bool IsRootAccessAllowed(Profile* profile) override;

  void set_allowed(bool allowed) {
    allowed_set_ = true;
    allowed_ = allowed;
  }
  void set_ui_allowed(bool allowed) {
    ui_allowed_set_ = true;
    ui_allowed_ = allowed;
  }
  void set_enabled(bool enabled) {
    enabled_set_ = true;
    enabled_ = enabled;
  }
  void set_export_import_ui_allowed(bool allowed) {
    export_import_ui_allowed_set_ = true;
    export_import_ui_allowed_ = allowed;
  }
  void set_root_access_allowed(bool allowed) {
    root_access_allowed_set_ = true;
    root_access_allowed_ = allowed;
  }

 private:
  // Original global static when this instance is created.  It is captured when
  // FakeCrostiniFeatures is created and replaced at destruction.
  CrostiniFeatures* original_features_;

  bool allowed_set_ = false;
  bool allowed_ = false;
  bool ui_allowed_set_ = false;
  bool ui_allowed_ = false;
  bool enabled_ = false;
  bool enabled_set_ = false;
  bool export_import_ui_allowed_set_ = false;
  bool export_import_ui_allowed_ = false;
  bool root_access_allowed_set_ = false;
  bool root_access_allowed_ = false;
};

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_FAKE_CROSTINI_FEATURES_H_
